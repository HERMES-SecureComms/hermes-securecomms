"""argparse entry point and shell; all authorization logic lives in the core."""

import argparse
import json
import shlex
from collections.abc import Sequence

from . import __version__
from .events import Event, EventType
from .simulator import Simulator, load_configuration, run_scenario

HELP = """Commands:
  credential attach ID       Present a registered or unknown credential
  credential detach          Remove credential and start eligible grace period
  credential revoke ID       Revoke a registered credential for this simulation
  ptt press | ptt release    Hold or release simulated PTT
  time advance MILLISECONDS  Advance virtual time (non-negative integer)
  device revoke | device restore
  system fault | system recover | system reboot
  status                     Show current state and deny reason
  events                     Show structured event history (JSON lines)
  help                       Show this command list
  exit | quit                Leave shell
"""

COMMANDS = {
    ("credential", "attach"): ("attach_credential", "credential_id"),
    ("credential", "detach"): ("detach_credential", None),
    ("credential", "revoke"): ("revoke_credential", "credential_id"),
    ("ptt", "press"): ("ptt_press", None),
    ("ptt", "release"): ("ptt_release", None),
    ("time", "advance"): ("advance_time", "milliseconds"),
    ("device", "revoke"): ("revoke_device", None),
    ("device", "restore"): ("restore_device", None),
    ("system", "fault"): ("system_fault", None),
    ("system", "recover"): ("system_recover", None),
    ("system", "reboot"): ("system_reboot", None),
}


def render_event(event: Event) -> list[str]:
    kind = event.event_type
    messages = {
        EventType.AUTH_FAILED: ["AUTH FAILED"],
        EventType.CREDENTIAL_LOST: ["CREDENTIAL LOST"],
        EventType.AUTH_EXPIRED: ["AUTH EXPIRED", "TX PERMISSION REVOKED"],
        EventType.AUTHORIZATION_INVALIDATED: ["ACTIVE AUTHORIZATION INVALIDATED"],
        EventType.PTT_PRESSED: ["PTT PRESSED"],
        EventType.PTT_RELEASED: ["PTT RELEASED"],
        EventType.TX_ALLOWED: ["TX ALLOWED"],
        EventType.TX_DENIED: ["TX DENIED"],
        EventType.TX_DISABLED: ["TX DISABLED"],
        EventType.DEVICE_REVOKED: ["DEVICE REVOKED"],
        EventType.DEVICE_RESTORED: ["DEVICE RESTORED", "CREDENTIAL REAUTHENTICATION REQUIRED"],
        EventType.CREDENTIAL_REVOKED: [f"CREDENTIAL REVOKED: {event.credential_id}"],
        EventType.SYSTEM_FAULT: ["SYSTEM FAULT", "ENTERING FAIL-CLOSED MODE"],
        EventType.SYSTEM_RECOVERED: ["SYSTEM RECOVERED", "CREDENTIAL REAUTHENTICATION REQUIRED"],
        EventType.SYSTEM_REBOOT: ["SYSTEM REBOOT", "SYSTEM HEALTHY"],
    }
    if kind is EventType.CREDENTIAL_ATTACHED:
        lines = [f"CREDENTIAL DETECTED: {event.credential_id}"]
    elif kind is EventType.AUTH_SUCCESS:
        lines = ["AUTH SUCCESS", f"OPERATOR: {event.operator_id}"]
    elif kind is EventType.GRACE_STARTED:
        lines = ["ENTERING GRACE PERIOD", f"GRACE PERIOD: {event.milliseconds} ms"]
    elif kind is EventType.TIME_ADVANCED:
        lines = [f"TIME +{event.milliseconds} ms"]
    else:
        lines = messages[kind].copy()
    if event.reason and kind in {EventType.AUTH_FAILED, EventType.TX_DENIED}:
        lines.append(f"REASON: {event.reason.removeprefix('DENY_')}")
    return [f"[HERMES] {line}" for line in lines]


def print_events(events: tuple[Event, ...]) -> None:
    for event in events:
        for line in render_event(event):
            print(line)


def print_status(simulator: Simulator) -> None:
    status = simulator.status()
    print("HERMES STATUS\n--------------------------------")
    fields = {
        "Device ID": status["device_id"],
        "Device State": "REVOKED" if status["device_revoked"] else "ACTIVE",
        "System Health": "HEALTHY" if status["system_healthy"] else "FAULT",
        "Credential": status["credential_id"] or "(detached)",
        "Operator": status["operator_id"] or "-",
        "Auth State": status["auth_state"],
        "PTT": status["ptt_state"],
        "TX": "ALLOWED" if status["tx_enabled"] else "DENIED (DISABLED)",
        "Decision": status["decision"],
        "Time": f"{status['time_ms']} ms",
        "Grace Deadline": status["grace_deadline_ms"] if status["grace_deadline_ms"] is not None else "-",
    }
    for label, value in fields.items():
        print(f"{label:16}: {value}")
    print("--------------------------------")


def execute_command(simulator: Simulator, line: str) -> bool:
    """Execute one shell line; False means a requested normal exit."""
    words = shlex.split(line)
    if not words:
        return True
    if words in (["exit"], ["quit"]):
        return False
    if words == ["help"]:
        print(HELP)
    elif words == ["status"]:
        print_status(simulator)
    elif words == ["events"]:
        for event in simulator.machine.events:
            print(json.dumps(event.to_dict(), ensure_ascii=False))
    else:
        command = COMMANDS.get(tuple(words[:2]))
        if command is None:
            raise ValueError("unknown command; use help")
        action, argument = command
        if len(words) != (3 if argument else 2):
            raise ValueError("wrong number of arguments; use help")
        arguments = {}
        if argument:
            arguments[argument] = int(words[2]) if argument == "milliseconds" else words[2]
        first_event = len(simulator.machine.events)
        snapshot = simulator.execute(action, **arguments)
        print_events(simulator.machine.events[first_event:])
        if action == "advance_time" and snapshot["auth_state"] == "GRACE_PERIOD":
            print("[HERMES] AUTH STILL VALID IN GRACE PERIOD")
    return True


def shell(simulator: Simulator) -> int:
    print(f"HERMES Simulator v{__version__}\nPress. Authenticate. Communicate.")
    print("Virtual clock: use 'time advance MS'. Type 'help' for commands.")
    while True:
        try:
            line = input("> ")
        except (EOFError, KeyboardInterrupt):
            print()
            return 0
        try:
            if not execute_command(simulator, line):
                return 0
        except ValueError as error:
            print(f"[HERMES] ERROR: {error}")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="HERMES software-only PTT simulator")
    parser.add_argument("--version", action="version", version=__version__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("shell", "run"):
        subparser = subparsers.add_parser(name)
        subparser.add_argument("--config", help="JSON configuration (default: built-in example)")
        if name == "run":
            subparser.add_argument("scenario", help="JSON scenario path")
    arguments = parser.parse_args(argv)
    try:
        simulator = Simulator(load_configuration(arguments.config))
        if arguments.command == "shell":
            return shell(simulator)
        result = run_scenario(arguments.scenario, simulator)
        print_events(simulator.machine.events)
        for step in result.steps:
            print(f"Step {step.index}: {step.action}: {'PASS' if step.passed else 'FAIL'}")
            for error in step.errors:
                print(f"  {error}")
        print(f"\nScenario: {result.name}\nSteps: {len(result.steps)}")
        print(f"Passed: {result.passed}\nFailed: {result.failed}")
        print(f"RESULT: {'PASS' if not result.failed else 'FAIL'}")
        return 1 if result.failed else 0
    except (OSError, ValueError) as error:
        parser.exit(2, f"[HERMES] ERROR: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
