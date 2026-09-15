from dataclasses import replace
from itertools import product
import random

import pytest

from hermes_simulator.models import AuthState, Credential, Device, PolicyContext, PttState, TxDecision
from hermes_simulator.policy import evaluate


def test_tc_sim_008_system_fault(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    state = sim.execute("system_fault")
    assert state["auth_state"] == "SYSTEM_FAILURE"
    assert state["decision"] == "DENY_SYSTEM_FAILURE"
    assert not state["session_valid"]
    assert not sim.execute("attach_credential", credential_id="GLOVE-001")["tx_enabled"]


def test_tc_sim_009_recover_requires_reauthentication(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("system_fault")
    state = sim.execute("system_recover")
    assert state["system_healthy"]
    assert state["decision"] == "DENY_REAUTHENTICATION_REQUIRED"
    assert state["auth_state"] == "UNAUTHENTICATED"
    sim.execute("ptt_release")
    assert not sim.execute("ptt_press")["tx_enabled"]
    assert sim.execute("attach_credential", credential_id="GLOVE-001")["decision"] == "ALLOW"


def test_tc_sim_010_reboot_clean_session(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("advance_time", milliseconds=4200)
    sim.execute("system_fault")
    count = len(sim.machine.events)
    state = sim.execute("system_reboot")
    assert state["system_healthy"]
    assert state["auth_state"] == "UNAUTHENTICATED"
    assert state["credential_id"] is None
    assert state["ptt_state"] == "RELEASED"
    assert not state["tx_enabled"]
    assert not state["session_valid"]
    assert state["time_ms"] == 4200
    assert len(sim.machine.events) > count


@pytest.mark.parametrize("restore_order", [True, False])
def test_fault_and_revoke_are_independent_barriers(sim, restore_order):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("system_fault")
    sim.execute("revoke_device")
    assert sim.status()["decision"] == "DENY_SYSTEM_FAILURE"
    first, second = ("restore_device", "system_recover") if restore_order else ("system_recover", "restore_device")
    assert not sim.execute(first)["tx_enabled"]
    assert not sim.execute(second)["tx_enabled"]
    assert not sim.status()["session_valid"]


def test_policy_all_boolean_gate_combinations():
    for healthy, device_revoked, authorized, cred_revoked, session, pressed in product((False, True), repeat=6):
        context = PolicyContext(
            Device(system_healthy=healthy, revoked=device_revoked),
            Credential("GLOVE-001", "OPERATOR-001", authorized, cred_revoked),
            "GLOVE-001", AuthState.AUTHORIZED,
            PttState.PRESSED if pressed else PttState.RELEASED,
            session, None, 0, TxDecision.DENY_NO_CREDENTIAL,
        )
        expected = healthy and not device_revoked and authorized and not cred_revoked and session and pressed
        assert (evaluate(context) is TxDecision.ALLOW) == expected


def test_policy_rechecks_deadline_even_without_state_machine_refresh(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    context = replace(sim.machine.context(), now_ms=1500)
    assert evaluate(context) is TxDecision.DENY_CREDENTIAL_EXPIRED


@pytest.mark.parametrize("fields", [
    {"auth_state": None},
    {"auth_state": "CORRUPTED"},
    {"auth_state": "AUTHORIZED"},
    {"ptt_state": None},
    {"now_ms": -1},
    {"now_ms": None},
    {"session_valid": None},
    {"attached_credential_id": "OTHER"},
    {"grace_deadline_ms": 2000},
    {"auth_state": AuthState.GRACE_PERIOD, "grace_deadline_ms": None},
    {"auth_state": AuthState.GRACE_PERIOD, "attached_credential_id": None, "grace_deadline_ms": "1500"},
    {"auth_state": AuthState.UNAUTHENTICATED, "denial_reason": TxDecision.ALLOW},
])
def test_policy_unknown_or_inconsistent_states_deny(sim, fields):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    assert evaluate(replace(sim.machine.context(), **fields)) is not TxDecision.ALLOW


def test_deterministic_long_action_sequence_preserves_safety(sim):
    rng = random.Random(410)
    actions = [
        ("attach_credential", {"credential_id": "GLOVE-001"}),
        ("attach_credential", {"credential_id": "UNKNOWN"}),
        ("detach_credential", {}), ("ptt_press", {}), ("ptt_release", {}),
        ("advance_time", {"milliseconds": 1500}),
        ("revoke_device", {}), ("restore_device", {}),
        ("system_fault", {}), ("system_recover", {}), ("system_reboot", {}),
        ("revoke_credential", {"credential_id": "GLOVE-002"}),
    ]
    for _ in range(2000):
        action, arguments = rng.choice(actions)
        state = sim.execute(action, **arguments)
        if state["tx_enabled"]:
            assert state["system_healthy"] and not state["device_revoked"]
            assert state["session_valid"] and state["ptt_state"] == "PRESSED"
            assert sim.machine.credential.authorized and not sim.machine.credential.revoked
            if state["auth_state"] == "GRACE_PERIOD":
                assert state["time_ms"] < state["grace_deadline_ms"]
            else:
                assert state["auth_state"] == "AUTHORIZED"
                assert state["credential_id"] == "GLOVE-001"
