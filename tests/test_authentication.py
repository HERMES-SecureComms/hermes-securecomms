import pytest

from hermes_simulator.clock import FakeClock
from hermes_simulator.models import Configuration
from hermes_simulator.simulator import Simulator


def test_tc_sim_002_unknown_credential(sim):
    sim.execute("attach_credential", credential_id="UNKNOWN-001")
    assert sim.execute("ptt_press")["decision"] == "DENY_UNKNOWN_CREDENTIAL"
    assert not sim.status()["session_valid"]


def test_tc_sim_003_no_credential(sim):
    assert sim.execute("ptt_press")["decision"] == "DENY_NO_CREDENTIAL"


def test_tc_sim_004_within_grace(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("detach_credential")
    sim.execute("advance_time", milliseconds=1499)
    state = sim.execute("ptt_press")
    assert state["decision"] == "ALLOW"
    assert state["auth_state"] == "GRACE_PERIOD"


def test_tc_sim_005_exact_grace_deadline(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("detach_credential")
    state = sim.execute("advance_time", milliseconds=1500)
    assert state["decision"] == "DENY_CREDENTIAL_EXPIRED"
    assert not state["session_valid"]
    assert state["auth_state"] == "UNAUTHENTICATED"


def test_duplicate_detach_does_not_extend_grace(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("detach_credential")
    sim.execute("advance_time", milliseconds=1000)
    assert sim.execute("detach_credential")["grace_deadline_ms"] == 1500
    assert sim.execute("advance_time", milliseconds=500)["decision"] == "DENY_CREDENTIAL_EXPIRED"


@pytest.mark.parametrize("elapsed", [1499, 1500, 1600])
def test_credential_return_reauthenticates(sim, elapsed):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    sim.execute("advance_time", milliseconds=elapsed)
    state = sim.execute("attach_credential", credential_id="GLOVE-001")
    assert state["decision"] == "ALLOW"
    assert state["auth_state"] == "AUTHORIZED"
    assert state["grace_deadline_ms"] is None


def test_unknown_replacement_cancels_existing_grace(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    state = sim.execute("attach_credential", credential_id="UNKNOWN")
    assert state["decision"] == "DENY_UNKNOWN_CREDENTIAL"
    assert state["grace_deadline_ms"] is None


@pytest.mark.parametrize("grace", [0, 20, 3000])
def test_configurable_grace(clock, grace):
    sim = Simulator(Configuration(grace_period_ms=grace), clock)
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    assert sim.execute("advance_time", milliseconds=grace)["decision"] == "DENY_CREDENTIAL_EXPIRED"


def test_read_refreshes_expiry_after_direct_fake_clock_advance(sim, clock):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("detach_credential")
    clock.advance(1500)
    assert not sim.status()["tx_enabled"]


@pytest.mark.parametrize("invalid", [-1, 1.5, True, "1000", None])
def test_clock_rejects_invalid_advances(invalid):
    clock = FakeClock()
    with pytest.raises(ValueError):
        clock.advance(invalid)
    assert clock.now_ms == 0


def test_known_but_unauthorized_credential(sim):
    sim.execute("attach_credential", credential_id="BLOCKED")
    assert sim.execute("ptt_press")["decision"] == "DENY_CREDENTIAL_UNAUTHORIZED"

