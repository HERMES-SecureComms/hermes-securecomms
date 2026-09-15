"""Application facade, configuration loading, and reproducible scenario runner."""

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .clock import Clock, VirtualClock, require_milliseconds
from .models import (
    AuthState, Configuration, Credential, PttState, TxDecision, require_identifier,
)
from .state_machine import StateMachine

ACTION_ARGUMENTS = {
    "attach_credential": {"credential_id"},
    "detach_credential": set(),
    "revoke_credential": {"credential_id"},
    "advance_time": {"milliseconds"},
    "ptt_press": set(),
    "ptt_release": set(),
    "revoke_device": set(),
    "restore_device": set(),
    "system_fault": set(),
    "system_recover": set(),
    "system_reboot": set(),
}


def load_configuration(path: str | Path | None = None) -> Configuration:
    if path is None:
        return Configuration()
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) - {"device_id", "grace_period_ms", "credentials"}:
        raise ValueError("configuration must be an object with only device_id/grace_period_ms/credentials")
    fields = dict(data)
    if "credentials" in data:
        if not isinstance(data["credentials"], list):
            raise ValueError("credentials must be a list")
        credentials = []
        for entry in data["credentials"]:
            if (
                not isinstance(entry, dict)
                or not {"credential_id", "operator_id"} <= set(entry)
                or set(entry) - {"credential_id", "operator_id", "authorized", "revoked"}
            ):
                raise ValueError("invalid credential configuration")
            credentials.append(Credential(**entry))
        fields["credentials"] = tuple(credentials)
    return Configuration(**fields)


def validate_action(action: object, arguments: dict[str, Any]) -> str:
    if not isinstance(action, str) or action not in ACTION_ARGUMENTS:
        raise ValueError(f"unknown action: {action!r}")
    if set(arguments) != ACTION_ARGUMENTS[action]:
        expected = ", ".join(sorted(ACTION_ARGUMENTS[action])) or "none"
        raise ValueError(f"{action} requires exactly these arguments: {expected}")
    if "credential_id" in arguments:
        require_identifier(arguments["credential_id"], "credential_id")
    if "milliseconds" in arguments:
        require_milliseconds(arguments["milliseconds"])
    return action


class Simulator:
    def __init__(self, configuration: Configuration | None = None, clock: Clock | None = None) -> None:
        self.machine = StateMachine(configuration or Configuration(), clock or VirtualClock())

    def execute(self, action: str, **arguments: Any) -> dict[str, Any]:
        validate_action(action, arguments)
        # Only the explicit command vocabulary is dispatchable.
        getattr(self.machine, action)(**arguments)
        return self.status()

    def status(self) -> dict[str, Any]:
        decision = self.machine.decision
        credential = self.machine.credential
        return {
            "device_id": self.machine.device.device_id,
            "device_revoked": self.machine.device.revoked,
            "system_healthy": self.machine.device.system_healthy,
            "credential_id": self.machine.attached_credential_id,
            "operator_id": credential.operator_id if credential else None,
            "auth_state": self.machine.auth_state.value,
            "session_valid": self.machine.session_valid,
            "ptt_state": self.machine.ptt_state.value,
            "tx_enabled": decision is TxDecision.ALLOW,
            "decision": decision.value,
            "time_ms": self.machine.clock.now_ms,
            "grace_deadline_ms": self.machine.grace_deadline_ms,
        }


def validate_expectation(expectation: object) -> dict[str, Any]:
    if not isinstance(expectation, dict):
        raise ValueError("expect must be an object")
    enums = {
        "auth_state": {state.value for state in AuthState},
        "ptt_state": {state.value for state in PttState},
        "decision": {decision.value for decision in TxDecision},
    }
    for key, value in expectation.items():
        if key in enums:
            if not isinstance(value, str) or value not in enums[key]:
                raise ValueError(f"invalid expected {key}: {value!r}")
        elif key in {"tx_enabled", "session_valid", "device_revoked", "system_healthy"}:
            if type(value) is not bool:
                raise ValueError(f"expected {key} must be a boolean")
        elif key in {"time_ms", "grace_deadline_ms"}:
            if key != "grace_deadline_ms" or value is not None:
                require_milliseconds(value, key)
        elif key in {"device_id", "credential_id", "operator_id"}:
            if key == "device_id" or value is not None:
                require_identifier(value, key)
        else:
            raise ValueError(f"unknown expectation field: {key}")
    return expectation


def load_scenario(path: str | Path) -> dict[str, Any]:
    scenario = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(scenario, dict) or set(scenario) != {"name", "steps"}:
        raise ValueError("scenario requires exactly name and steps")
    require_identifier(scenario["name"], "scenario name")
    if not isinstance(scenario["steps"], list) or not scenario["steps"]:
        raise ValueError("scenario steps must be a non-empty list")
    # Validate the entire document before mutating the simulator.
    for index, step in enumerate(scenario["steps"], 1):
        if not isinstance(step, dict):
            raise ValueError(f"step {index} must be an object")
        validate_action(step.get("action"), {
            key: value for key, value in step.items() if key not in {"action", "expect"}
        })
        validate_expectation(step.get("expect", {}))
    return scenario


@dataclass(frozen=True)
class StepResult:
    index: int
    action: str
    passed: bool
    errors: tuple[str, ...]
    status: dict[str, Any]


@dataclass(frozen=True)
class ScenarioResult:
    name: str
    steps: tuple[StepResult, ...]

    @property
    def passed(self) -> int:
        return sum(step.passed for step in self.steps)

    @property
    def failed(self) -> int:
        return len(self.steps) - self.passed


def run_scenario(path: str | Path, simulator: Simulator | None = None) -> ScenarioResult:
    scenario = load_scenario(path)
    simulator = simulator or Simulator()
    results = []
    for index, step in enumerate(scenario["steps"], 1):
        errors = []
        arguments = {key: value for key, value in step.items() if key not in {"action", "expect"}}
        try:
            snapshot = simulator.execute(step["action"], **arguments)
        except ValueError as error:
            errors.append(str(error))
            snapshot = simulator.status()
        for key, expected in step.get("expect", {}).items():
            if snapshot[key] != expected:
                errors.append(f"{key}: expected {expected!r}, got {snapshot[key]!r}")
        # Basic invariant even for user scenarios without explicit assertions.
        if snapshot["tx_enabled"] and not (
            snapshot["system_healthy"] and not snapshot["device_revoked"]
            and snapshot["session_valid"] and snapshot["ptt_state"] == "PRESSED"
            and snapshot["auth_state"] in {"AUTHORIZED", "GRACE_PERIOD"}
        ):
            errors.append("fail-closed invariant violated")
        results.append(StepResult(index, step["action"], not errors, tuple(errors), snapshot))
    return ScenarioResult(scenario["name"], tuple(results))

