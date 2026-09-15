import json
from pathlib import Path
import subprocess
import sys

import pytest

from hermes_simulator.cli import execute_command

ROOT = Path(__file__).resolve().parents[1]


def test_interactive_demo_as_real_subprocess():
    commands = "\n".join([
        "status", "credential attach GLOVE-001", "ptt press", "ptt release",
        "credential detach", "time advance 1000", "time advance 600",
        "ptt press", "status", "events", "exit", "",
    ])
    process = subprocess.run(
        [sys.executable, "-m", "hermes_simulator.cli", "shell"],
        input=commands, text=True, capture_output=True, cwd=ROOT, check=False,
    )
    assert process.returncode == 0, process.stderr
    for message in [
        "UNAUTHENTICATED", "AUTH SUCCESS", "TX ALLOWED", "PTT RELEASED",
        "TX DISABLED", "ENTERING GRACE PERIOD", "AUTH STILL VALID IN GRACE PERIOD",
        "AUTH EXPIRED", "REASON: CREDENTIAL_EXPIRED", '"event_type": "AUTH_EXPIRED"',
    ]:
        assert message in process.stdout


def test_events_command_emits_structured_json(sim, capsys):
    execute_command(sim, "credential attach GLOVE-001")
    capsys.readouterr()
    execute_command(sim, "events")
    entries = [json.loads(line) for line in capsys.readouterr().out.splitlines()]
    assert entries[0]["event_type"] == "CREDENTIAL_ATTACHED"
    assert {"timestamp", "event_type", "device_id", "credential_id", "operator_id", "reason"} <= entries[0].keys()


@pytest.mark.parametrize("line", ["ptt press extra", "time advance -1", "time advance 1.5", "credential attach", "invalid", 'credential attach ""'])
def test_invalid_shell_command_does_not_mutate(sim, line):
    before = sim.status()
    with pytest.raises(ValueError):
        execute_command(sim, line)
    assert sim.status() == before


def test_shell_continues_after_input_error():
    process = subprocess.run(
        [sys.executable, "-m", "hermes_simulator.cli", "shell"],
        input="time advance -1\nstatus\nexit\n", text=True,
        capture_output=True, cwd=ROOT, check=False,
    )
    assert process.returncode == 0
    assert "[HERMES] ERROR" in process.stdout
    assert "HERMES STATUS" in process.stdout


def test_invalid_scenario_returns_input_error(tmp_path):
    path = tmp_path / "invalid.json"
    path.write_text("{broken")
    process = subprocess.run(
        [sys.executable, "-m", "hermes_simulator.cli", "run", str(path)],
        text=True, capture_output=True, cwd=ROOT, check=False,
    )
    assert process.returncode == 2
    assert "[HERMES] ERROR" in process.stderr


@pytest.mark.parametrize("command", ["device revoke", "system fault", "system reboot"])
@pytest.mark.parametrize("active", [False, True])
def test_fail_closed_commands_log_tx_disabled_exactly_once(sim, capsys, command, active):
    if active:
        execute_command(sim, "credential attach GLOVE-001")
        execute_command(sim, "ptt press")
    capsys.readouterr()
    execute_command(sim, command)
    output = capsys.readouterr().out
    assert output.count("[HERMES] TX DISABLED") == 1
    assert not sim.status()["tx_enabled"]
