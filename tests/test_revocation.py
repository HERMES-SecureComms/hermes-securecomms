import pytest


@pytest.mark.parametrize("detached", [False, True])
def test_tc_sim_007_device_revocation_overrides_active_session(sim, detached):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    if detached:
        sim.execute("detach_credential")
    state = sim.execute("revoke_device")
    assert state["auth_state"] == "REVOKED"
    assert state["decision"] == "DENY_DEVICE_REVOKED"
    assert not state["session_valid"]
    sim.execute("attach_credential", credential_id="GLOVE-002")
    assert sim.execute("ptt_press")["decision"] == "DENY_DEVICE_REVOKED"


@pytest.mark.parametrize("detached", [False, True])
def test_tc_sim_011_active_credential_revocation(sim, detached):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    if detached:
        sim.execute("detach_credential")
    state = sim.execute("revoke_credential", credential_id="GLOVE-001")
    assert state["decision"] == "DENY_CREDENTIAL_REVOKED"
    assert not state["session_valid"]
    assert state["auth_state"] == "REVOKED"
    assert not sim.execute("attach_credential", credential_id="GLOVE-001")["tx_enabled"]


def test_unrelated_revocation_does_not_revoke_current_session(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    assert sim.execute("revoke_credential", credential_id="GLOVE-002")["decision"] == "ALLOW"


def test_device_restore_requires_explicit_reattach(sim):
    sim.execute("attach_credential", credential_id="GLOVE-001")
    sim.execute("ptt_press")
    sim.execute("revoke_device")
    state = sim.execute("restore_device")
    assert state["decision"] == "DENY_REAUTHENTICATION_REQUIRED"
    sim.execute("advance_time", milliseconds=10000)
    assert not sim.status()["tx_enabled"]
    assert sim.execute("attach_credential", credential_id="GLOVE-001")["decision"] == "ALLOW"


def test_reboot_preserves_device_and_credential_revocation(sim):
    sim.execute("revoke_device")
    sim.execute("revoke_credential", credential_id="GLOVE-001")
    assert sim.execute("system_reboot")["decision"] == "DENY_DEVICE_REVOKED"
    sim.execute("restore_device")
    sim.execute("attach_credential", credential_id="GLOVE-001")
    assert sim.execute("ptt_press")["decision"] == "DENY_CREDENTIAL_REVOKED"


def test_revoking_unknown_id_reports_error_without_state_change(sim):
    before = sim.status()
    with pytest.raises(ValueError, match="unregistered"):
        sim.execute("revoke_credential", credential_id="UNKNOWN")
    assert sim.status() == before

