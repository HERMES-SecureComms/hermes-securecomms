# HERMES Test Plan

> Current security design: [v0.2 architecture](V0_2_ARCHITECTURE.md) and [security tests](SECURITY_TEST_PLAN.md).

프로젝트를 개발 의존성과 함께 설치한 뒤 루트에서 `pytest`를 실행한다.
실제 sleep이나 하드웨어는 필요 없다. 핵심 fixture는 `FakeClock`과 예제
Credential 두 개, 비허용 Credential 한 개를 갖는 Simulator다.

## 필수 수용 테스트

| ID | 조건 | 기대 결과 | 자동 테스트 위치 |
|---|---|---|---|
| TC-SIM-001 | 정상 Credential + PTT | ALLOW; release 후 disable | test_ptt.py::test_tc_sim_001_authorized_ptt |
| TC-SIM-002 | 미등록 Credential + PTT | DENY_UNKNOWN_CREDENTIAL | test_authentication.py::test_tc_sim_002_unknown_credential |
| TC-SIM-003 | Credential 없음 + PTT | DENY_NO_CREDENTIAL | test_authentication.py::test_tc_sim_003_no_credential |
| TC-SIM-004 | 분리 후 1499 ms | GRACE_PERIOD, ALLOW | test_authentication.py::test_tc_sim_004_within_grace |
| TC-SIM-005 | 분리 후 정확히 1500 ms | UNAUTHENTICATED, DENY_CREDENTIAL_EXPIRED | test_authentication.py::test_tc_sim_005_exact_grace_deadline |
| TC-SIM-006 | PTT 누른 채 Credential 소실 | 다음 press 없이 만료 시 TX disable | test_ptt.py::test_tc_sim_006_held_ptt_disabled_at_expiry |
| TC-SIM-007 | 활성/유예 세션에서 device revoke | REVOKED, 항상 deny | test_revocation.py::test_tc_sim_007_device_revocation_overrides_active_session |
| TC-SIM-008 | 활성 세션 중 system fault | SYSTEM_FAILURE, 항상 deny | test_fail_closed.py::test_tc_sim_008_system_fault |
| TC-SIM-009 | fault 후 recover | healthy이나 재인증 전 deny | test_fail_closed.py::test_tc_sim_009_recover_requires_reauthentication |
| TC-SIM-010 | 활성/장애 상태에서 reboot | 세션/부착 초기화, PTT RELEASED, deny | test_fail_closed.py::test_tc_sim_010_reboot_clean_session |
| TC-SIM-011 | 사용 중 Credential revoke | 즉시 세션 무효화 | test_revocation.py::test_tc_sim_011_active_credential_revocation |
| TC-SIM-012 | 반복 press/release | 조건 없는 enable 및 중복 TX 전환 없음 | test_ptt.py::test_tc_sim_012_repeated_ptt_press_release |

## 추가 검증

- 장치/credential/세션/PTT boolean gate 64개 조합에 대해 ALLOW 조건 확인
- 2,000개의 seed 고정 action sequence에서 TX 허용 invariant 확인
- grace 0/20/3000 ms, 경계 전후 재부착, 중복 detach deadline 유지
- unknown replacement 시 grace 즉시 폐기
- fault와 device revoke의 중첩 및 복구 순서
- reboot에서 device/credential 철회 보존
- 다른 Credential 철회 시 현재 정상 세션 유지
- 상태 머신 refresh 없이도 policy의 만료 판단 확인
- 알 수 없거나 모순된 상태에서 fail-closed 확인
- 만료 이벤트의 timestamp/credential/operator와 중복 방지 확인
- JSON 전체 선검증, 잘못된 expectation, action 인자, config type 거부
- 틀린 expectation이 실제 scenario FAIL 및 프로세스 종료 코드 1로 이어지는지 확인
- CLI subprocess로 제공 시나리오 5개와 실제 shell demo 실행
- CLI 오류 후 shell 지속, status/events 출력 및 종료 코드 2 입력 오류 검증

## 시나리오 검증 명령

```bash
python -m hermes_simulator.cli run scenarios/normal_operation.json
python -m hermes_simulator.cli run scenarios/stolen_radio.json
python -m hermes_simulator.cli run scenarios/credential_loss_during_tx.json
python -m hermes_simulator.cli run scenarios/unknown_credential.json
python -m hermes_simulator.cli run scenarios/revoked_device.json
```

제공 시나리오 각 단계에는 `expect`가 있다. 허용/거부만으로 충분하지 않은
경우 auth_state, session_valid, ptt_state도 확인한다. `RESULT: PASS`는 모든
지정 기대값 및 기본 invariant의 통과를 뜻한다. 실제 radio/NFC/암호 보안
검증을 수행했다는 뜻이 아니다.

## 수동 CLI 재현

`python -m hermes_simulator.cli shell`에서 README 예제 session을 그대로 입력한다.
추가로 TX 활성 상태에서 `system fault`, `system recover`, `ptt press`를
입력하고 명시적 attach 전에는 TX가 켜지지 않는지 확인한다.
`device revoke` 후 `system reboot`해도 device가 REVOKED인지 확인한다.

환경, 테스트 개수, 실행 결과는 `VALIDATION.md`에 남긴다. 향후 실물 trace와
비교할 때에는 가상 detach와 last NFC read 시각의 차이를 먼저 정의해야 한다.

