import json
from pathlib import Path
import subprocess
import sys

import pytest

from hermes_simulator.simulator import load_configuration, run_scenario

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = sorted((ROOT / "scenarios").glob("*.json"))


@pytest.mark.parametrize("path", SCENARIOS, ids=lambda p: p.stem)
def test_all_shipped_scenarios_have_assertions_and_pass(path):
    data = json.loads(path.read_text())
    assert all(step.get("expect") for step in data["steps"])
    result = run_scenario(path)
    assert result.failed == 0
    assert result.passed == len(data["steps"])


@pytest.mark.parametrize("path", SCENARIOS, ids=lambda p: p.stem)
def test_real_cli_scenario_command(path):
    process = subprocess.run(
        [sys.executable, "-m", "hermes_simulator.cli", "run", str(path)],
        text=True, capture_output=True, cwd=ROOT, check=False,
    )
    assert process.returncode == 0, process.stderr
    assert "RESULT: PASS" in process.stdout
    assert "Failed: 0" in process.stdout


def test_failed_expectation_is_a_real_failure(tmp_path):
    path = tmp_path / "bad-expect.json"
    path.write_text(json.dumps({"name": "bad-expect", "steps": [
        {"action": "ptt_press", "expect": {"decision": "ALLOW"}},
    ]}))
    result = run_scenario(path)
    assert result.failed == 1
    assert "DENY_NO_CREDENTIAL" in result.steps[0].errors[0]
    process = subprocess.run(
        [sys.executable, "-m", "hermes_simulator.cli", "run", str(path)],
        text=True, capture_output=True, cwd=ROOT, check=False,
    )
    assert process.returncode == 1
    assert "RESULT: FAIL" in process.stdout


@pytest.mark.parametrize("step", [
    {"action": "__init__"},
    {"action": "advance_time", "milliseconds": -1},
    {"action": "advance_time", "milliseconds": True},
    {"action": "ptt_press", "extra": "ignore-me"},
    {"action": "ptt_press", "expect": {"typo": False}},
    {"action": "ptt_press", "expect": {"tx_enabled": "false"}},
    {"action": "ptt_press", "expect": {"decision": "UNKNOWN_REASON"}},
])
def test_scenario_prevalidation_avoids_partial_execution(sim, tmp_path, step):
    path = tmp_path / "invalid.json"
    path.write_text(json.dumps({"name": "invalid", "steps": [
        {"action": "attach_credential", "credential_id": "GLOVE-001"}, step,
    ]}))
    with pytest.raises(ValueError):
        run_scenario(path, sim)
    assert sim.machine.events == ()


def test_example_configuration():
    config = load_configuration(ROOT / "config.example.json")
    assert config.grace_period_ms == 1500
    assert len(config.credentials) == 3


@pytest.mark.parametrize("config", [
    {"grace_period_ms": -1}, {"grace_period_ms": True}, {"grace_period_ms": 1.5},
    {"unknown_key": 1}, {"credentials": "invalid"},
    {"credentials": [{"credential_id": "G", "operator_id": "O", "authorized": "false"}]},
    {"credentials": [{"credential_id": "G", "operator_id": "O"}] * 2},
])
def test_invalid_configuration_rejected(tmp_path, config):
    path = tmp_path / "config.json"
    path.write_text(json.dumps(config))
    with pytest.raises(ValueError):
        load_configuration(path)


def test_action_only_scenarios_are_supported(tmp_path):
    path = tmp_path / "actions.json"
    path.write_text(json.dumps({"name": "actions-only", "steps": [{"action": "ptt_press"}]}))
    assert run_scenario(path).passed == 1

