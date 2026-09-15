# Changed authored files

Computed from the before-change SHA-256 inventory. No Git commands used.
Generated `.pio`, native build, Python cache and package metadata are excluded.

## hermes-poc

| Change | Path |
|---|---|
| Changed | `.gitignore` |
| Changed | `README.md` |
| Changed | `SECURITY.md` |
| Changed | `docs/ARCHITECTURE.md` |
| Added | `docs/AUTH_PROTOCOL.md` |
| Added | `docs/KEY_MANAGEMENT.md` |
| Added | `docs/MIGRATION_V0_1_TO_V0_2.md` |
| Added | `docs/REVOCATION.md` |
| Changed | `docs/ROADMAP.md` |
| Added | `docs/ROADMAP_V0_1.md` |
| Added | `docs/SECURITY_TEST_PLAN.md` |
| Added | `docs/SECURITY_V0_1.md` |
| Added | `docs/SESSION_MODEL.md` |
| Changed | `docs/TEST_PLAN.md` |
| Added | `docs/THREAT_MODEL_V0_2.md` |
| Added | `docs/V0_1_README.md` |
| Added | `docs/V0_2_ARCHITECTURE.md` |
| Added | `docs/VALIDATION_REPORT.md` |
| Added | `docs/validation/baseline-manifest.json` |
| Added | `docs/validation/counter_rollback.log` |
| Added | `docs/validation/credential_revoked.log` |
| Added | `docs/validation/device_binding_failure.log` |
| Added | `docs/validation/firmware-poc-build.log` |
| Added | `docs/validation/firmware-simulator-build.log` |
| Added | `docs/validation/native-parity.log` |
| Added | `docs/validation/python-tests.log` |
| Added | `docs/validation/replay_attack.log` |
| Added | `docs/validation/revocation_list_rollback.log` |
| Added | `docs/validation/scenarios.json` |
| Added | `docs/validation/secure_normal_operation.log` |
| Added | `docs/validation/session_expired_during_tx.log` |
| Added | `docs/validation/source-manifest.json` |
| Added | `docs/validation/system_fault_during_session.log` |
| Added | `docs/validation/tampered_revocation_list.log` |
| Added | `docs/validation/workspace-audit.json` |
| Changed | `firmware/include/config.h` |
| Added | `firmware/lib/hermes_security/src/codec.cpp` |
| Added | `firmware/lib/hermes_security/src/crypto_backend.cpp` |
| Added | `firmware/lib/hermes_security/src/hermes_security.cpp` |
| Added | `firmware/lib/hermes_security/src/hermes_security.h` |
| Changed | `firmware/platformio.ini` |
| Added | `firmware/src/credential_transport.cpp` |
| Added | `firmware/src/credential_transport.h` |
| Changed | `firmware/src/main.cpp` |
| Changed | `firmware/src/nfc_auth.cpp` |
| Changed | `firmware/src/nfc_auth.h` |
| Added | `firmware/src/nvs_state_store.h` |
| Changed | `firmware/src/ptt_controller.cpp` |
| Changed | `firmware/src/ptt_controller.h` |
| Added | `firmware/src/secure_runtime.cpp` |
| Added | `firmware/test/fixtures/mock_provisioning.h` |
| Added | `firmware/test/fixtures/vectors.h` |
| Added | `firmware/test/native/run.sh` |
| Added | `firmware/test/native/security_test.cpp` |
| Added | `firmware/test/native/stubs/Arduino.h` |
| Added | `docs/CHANGED_FILES.md` |

## hermes-simulator

| Change | Path |
|---|---|
| Changed | `.gitignore` |
| Changed | `README.md` |
| Changed | `SECURITY.md` |
| Changed | `config.example.json` |
| Added | `config.uid-poc.example.json` |
| Changed | `docs/ARCHITECTURE.md` |
| Added | `docs/AUTH_PROTOCOL.md` |
| Added | `docs/KEY_MANAGEMENT.md` |
| Added | `docs/MIGRATION_V0_1_TO_V0_2.md` |
| Added | `docs/REVOCATION.md` |
| Changed | `docs/ROADMAP.md` |
| Added | `docs/ROADMAP_V0_1.md` |
| Added | `docs/SECURITY_TEST_PLAN.md` |
| Added | `docs/SECURITY_V0_1.md` |
| Added | `docs/SESSION_MODEL.md` |
| Changed | `docs/TEST_PLAN.md` |
| Added | `docs/THREAT_MODEL_V0_2.md` |
| Added | `docs/V0_1_README.md` |
| Added | `docs/V0_2_ARCHITECTURE.md` |
| Added | `docs/VALIDATION_REPORT.md` |
| Added | `docs/validation/baseline-manifest.json` |
| Added | `docs/validation/counter_rollback.log` |
| Added | `docs/validation/credential_revoked.log` |
| Added | `docs/validation/device_binding_failure.log` |
| Added | `docs/validation/firmware-poc-build.log` |
| Added | `docs/validation/firmware-simulator-build.log` |
| Added | `docs/validation/native-parity.log` |
| Added | `docs/validation/python-tests.log` |
| Added | `docs/validation/replay_attack.log` |
| Added | `docs/validation/revocation_list_rollback.log` |
| Added | `docs/validation/scenarios.json` |
| Added | `docs/validation/secure_normal_operation.log` |
| Added | `docs/validation/session_expired_during_tx.log` |
| Added | `docs/validation/source-manifest.json` |
| Added | `docs/validation/system_fault_during_session.log` |
| Added | `docs/validation/tampered_revocation_list.log` |
| Added | `docs/validation/workspace-audit.json` |
| Changed | `firmware/include/config.h` |
| Added | `firmware/lib/hermes_security/src/codec.cpp` |
| Added | `firmware/lib/hermes_security/src/crypto_backend.cpp` |
| Added | `firmware/lib/hermes_security/src/hermes_security.cpp` |
| Added | `firmware/lib/hermes_security/src/hermes_security.h` |
| Changed | `firmware/platformio.ini` |
| Added | `firmware/src/credential_transport.cpp` |
| Added | `firmware/src/credential_transport.h` |
| Changed | `firmware/src/main.cpp` |
| Changed | `firmware/src/nfc_auth.cpp` |
| Changed | `firmware/src/nfc_auth.h` |
| Added | `firmware/src/nvs_state_store.h` |
| Changed | `firmware/src/ptt_controller.cpp` |
| Changed | `firmware/src/ptt_controller.h` |
| Added | `firmware/src/secure_runtime.cpp` |
| Added | `firmware/test/fixtures/mock_provisioning.h` |
| Added | `firmware/test/fixtures/vectors.h` |
| Added | `firmware/test/native/run.sh` |
| Added | `firmware/test/native/security_test.cpp` |
| Added | `firmware/test/native/stubs/Arduino.h` |
| Changed | `pyproject.toml` |
| Added | `scenarios/counter_rollback.json` |
| Relocated (legacy scenarios) | `scenarios/credential_loss_during_tx.json` |
| Added | `scenarios/credential_revoked.json` |
| Added | `scenarios/device_binding_failure.json` |
| Relocated (legacy scenarios) | `scenarios/normal_operation.json` |
| Added | `scenarios/replay_attack.json` |
| Added | `scenarios/revocation_list_rollback.json` |
| Relocated (legacy scenarios) | `scenarios/revoked_device.json` |
| Added | `scenarios/secure_normal_operation.json` |
| Added | `scenarios/session_expired_during_tx.json` |
| Relocated (legacy scenarios) | `scenarios/stolen_radio.json` |
| Added | `scenarios/system_fault_during_session.json` |
| Added | `scenarios/tampered_revocation_list.json` |
| Relocated (legacy scenarios) | `scenarios/unknown_credential.json` |
| Added | `scenarios/v0_1/credential_loss_during_tx.json` |
| Added | `scenarios/v0_1/normal_operation.json` |
| Added | `scenarios/v0_1/revoked_device.json` |
| Added | `scenarios/v0_1/stolen_radio.json` |
| Added | `scenarios/v0_1/unknown_credential.json` |
| Changed | `src/hermes_simulator/__init__.py` |
| Changed | `src/hermes_simulator/cli.py` |
| Added | `src/hermes_simulator/legacy_cli.py` |
| Added | `src/hermes_simulator/legacy_simulator.py` |
| Added | `src/hermes_simulator/security/__init__.py` |
| Added | `src/hermes_simulator/security/crypto.py` |
| Added | `src/hermes_simulator/security/engine.py` |
| Added | `src/hermes_simulator/security/models.py` |
| Added | `src/hermes_simulator/security/revocation.py` |
| Added | `src/hermes_simulator/security/storage.py` |
| Changed | `src/hermes_simulator/simulator.py` |
| Changed | `tests/conftest.py` |
| Added | `tests/fixtures/crl_v10.json` |
| Added | `tests/fixtures/crl_v11.json` |
| Added | `tests/fixtures/crl_v9.json` |
| Added | `tests/fixtures/generate.py` |
| Added | `tests/fixtures/protocol_vector.json` |
| Added | `tests/fixtures/tampered_crl.json` |
| Added | `tests/fixtures/test_keys.json` |
| Changed | `tests/test_authentication.py` |
| Changed | `tests/test_cli.py` |
| Changed | `tests/test_scenarios.py` |
| Added | `tests/test_security_v02.py` |
| Added | `tools/validate_security.py` |
| Added | `docs/CHANGED_FILES.md` |

