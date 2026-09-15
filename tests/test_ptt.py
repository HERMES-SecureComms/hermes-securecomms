import pytest

from hermes_simulator.events import EventType


def test_tc_sim_001_authorized_ptt(sim):
    state = sim.execute("attach_credential", credential_id="GLOVE-001")
    assert not state["tx_enabled"]
    assert state["operator_id"] == "OPERATOR-001"
    assert sim.execute("ptt_press")["decision"] == "ALLOW"
    assert not sim.execute("ptt_release")["tx_enabled"]


def test_tc_sim_006_held_ptt_disabled_at_expiry(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    assert sim.execute("advance_time", milliseconds=1499)["tx_enabled"]
    state = sim.execute("advance_time", milliseconds=1)
    assert state["ptt_state"] == "PRESSED"
    assert not state["tx_enabled"]
    events = sim.machine.events
    expiration = next(e for e in events if e.event_type is EventType.AUTH_EXPIRED)
    disabled = next(e for e in events if e.event_type is EventType.TX_DISABLED)
    assert expiration.timestamp == disabled.timestamp == 1500
    assert disabled.reason == "DENY_CREDENTIAL_EXPIRED"


@pytest.mark.parametrize("credential", [None, "GLOVE-001", "UNKNOWN"])
def test_tc_sim_012_repeated_ptt_press_release(sim, credential):
    if credential:
        sim.execute("attach_credential", credential_id=credential)
    for _ in range(100):
        state = sim.execute("ptt_press")
        assert state["tx_enabled"] == (credential == "GLOVE-001")
        count = len(sim.machine.events)
        sim.execute("ptt_press")
        assert len(sim.machine.events) == count
        assert not sim.execute("ptt_release")["tx_enabled"]
        count = len(sim.machine.events)
        sim.execute("ptt_release")
        assert len(sim.machine.events) == count


def test_press_then_attach_uses_level_trigger_policy(sim):
    sim.execute("ptt_press")
    assert sim.execute("attach_credential", credential_id="GLOVE-001")["decision"] == "ALLOW"


def test_expiration_logs_once_and_audit_carries_session_identity(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("detach_credential")
    sim.execute("advance_time", milliseconds=1500)
    sim.execute("advance_time", milliseconds=10000)
    events = [e for e in sim.machine.events if e.event_type is EventType.AUTH_EXPIRED]
    assert len(events) == 1
    assert events[0].credential_id == "GLOVE-001"
    assert events[0].operator_id == "OPERATOR-001"
    assert events[0].device_id == "HPTT-001"

