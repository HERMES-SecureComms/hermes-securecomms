# 구현 검증 기록

검증일: 2026-09-09 (UTC)

## 실행 환경

- 작업 경로: `/workspace/hermes-simulator`
- Python: 3.14.6 (프로젝트 요구 버전은 3.12 이상)
- pytest: 9.1.1
- 검증용 가상 환경: `/tmp/hermes-simulator-venv`
- 설치: `python -m pip install -r requirements-dev.txt`
- 의존성 검사: `python -m pip check` → `No broken requirements found.`
- Git 명령을 실행하지 않았으며 프로젝트에 `.git` 디렉터리를 생성하지 않았다.

Python 3.12 전용 실행 환경에서는 별도로 실행하지 않았다. 실제 검증된 환경은
위의 Python 3.14.6이다. 설치된 패키지에는 외부 runtime dependency가 없다.

## pytest 결과

프로젝트 루트에서 검증용 환경의 `pytest`를 실제 실행했다.

```text
collected 97 items
tests/test_authentication.py ...................
tests/test_cli.py ................
tests/test_fail_closed.py ....................
tests/test_ptt.py .......
tests/test_revocation.py ........
tests/test_scenarios.py ...........................
97 passed in 0.66s
```

여기에는 TC-SIM-001~012, boolean gate 64개 조합, 2,000개 고정 난수 사건 순서,
실제 CLI subprocess, 제공 시나리오 5개, 기대값 실패 시 비정상 종료 테스트가 포함된다.
0.66초는 이 환경에서 한 번 실행한 테스트 suite 소요 시간이며 TX latency가 아니다.

## 별도 Scenario 실행 결과

`python -m hermes_simulator.cli run scenarios/<파일명>`을 실제 실행했다.
제공 파일에는 모든 단계에 기대 상태 검증이 있다.

| 파일 | Steps | Passed | Failed | Result |
|---|---:|---:|---:|---|
| normal_operation.json | 3 | 3 | 0 | PASS |
| stolen_radio.json | 6 | 6 | 0 | PASS |
| credential_loss_during_tx.json | 5 | 5 | 0 | PASS |
| unknown_credential.json | 2 | 2 | 0 | PASS |
| revoked_device.json | 6 | 6 | 0 | PASS |

`stolen_radio.json`의 실제 핵심 출력:

```text
[HERMES] CREDENTIAL LOST
[HERMES] ENTERING GRACE PERIOD
[HERMES] GRACE PERIOD: 1500 ms
[HERMES] TIME +1600 ms
[HERMES] AUTH EXPIRED
[HERMES] TX PERMISSION REVOKED
[HERMES] PTT PRESSED
[HERMES] TX DENIED
[HERMES] REASON: CREDENTIAL_EXPIRED
Scenario: stolen-radio
Steps: 6
Passed: 6
Failed: 0
RESULT: PASS
```

`credential_loss_during_tx.json`에서는 PTT가 PRESSED인 채 1000 + 500 ms 전진 후
AUTH_EXPIRED → TX_DISABLED → TX_DENIED가 기록되며, 마지막 decision은
DENY_CREDENTIAL_EXPIRED다. 다음 press 없이 차단됨을 검증했다.

## 실제 Interactive CLI 검증

`python -m hermes_simulator.cli shell`에 다음 입력을 전달했다.

```text
status
credential attach GLOVE-001
ptt press
ptt release
credential detach
time advance 1000
time advance 600
ptt press
status
credential attach GLOVE-001
system fault
system recover
status
credential attach GLOVE-001
device revoke
system reboot
status
exit
```

확인한 실제 상태:

- 부팅: UNAUTHENTICATED / RELEASED / DENY_NO_CREDENTIAL / t=0
- 1000 ms: AUTH STILL VALID IN GRACE PERIOD
- 1600 ms: DENY_CREDENTIAL_EXPIRED, PTT PRESSED, TX disabled
- 장애 복구: HEALTHY이나 UNAUTHENTICATED / DENY_REAUTHENTICATION_REQUIRED
- 장치 revoke 후 reboot: HEALTHY / REVOKED / RELEASED / DENY_DEVICE_REVOKED
- 전체 shell 프로세스 종료 코드: 0

## 코드 리뷰에서 확인한 사항

- 순수 `evaluate(context)` 함수가 허용 조건과 명확한 거부 이유를 관리한다.
- 상태 머신과 CLI 출력이 분리되어 같은 API를 테스트/시나리오에서 재사용한다.
- 분리된 Credential의 grace deadline은 중복 detach로 연장되지 않는다.
- 정상 Credential을 unknown으로 교체하면 기존 session/deadline을 즉시 폐기한다.
- fault/device revoke/current credential revoke는 활성/유예 세션을 무효화한다.
- recover/restore는 이전 인증을 자동 복구하지 않는다.
- reboot로 장치/credential 철회를 우회하지 못한다.
- event는 immutable 값이고 TX transition은 중복 PTT에서 반복 기록되지 않는다.
- 출력 점검 중 발견한 중복 TX DISABLED 메시지를 제거하고 idle/active 각각의
  revoke/fault/reboot 회귀 테스트를 추가했다.
- enum이 아닌 상태, 모순된 세션/ID/deadline과 잘못된 시각을 policy가 거부한다.
- 시나리오 입력을 실행 전에 검증하며 assertion mismatch는 PASS로 처리하지 않는다.

## 남은 TODO와 알려진 한계

이번 software-only v0.1 기능의 필수 구현/테스트는 완료했다. 아래는 후속 과제다.

- hardware last-seen timestamp와 simulator detach timestamp의 비교 규칙 정의
- 실제 firmware trace를 동일한 정책 기대값과 비교하는 절차 마련
- Python 3.12를 별도 matrix 환경에서 직접 검증
- production 복구 권한, 철회 영속성, PTT release-before-rearm 정책 결정
- Secure NFC/challenge-response/secure element를 도입할 때 새 위협 가정 검토

현재는 ID 등록 확인만 하므로 **Credential 동반 탈취와 복제를 구별하지 못한다.**
로컬 restore/recover는 시뮬레이션 제어 기능이며 production 관리 방식이 아니다.
철회/이벤트는 프로세스 메모리에만 남는다. 실제 NFC, GPIO, RF, radio 제어와
cryptographic authentication은 구현하지 않았고 실물 안전이나 신원 인증을 검증했다고
주장하지 않는다. [THREAT_MODEL.md](THREAT_MODEL.md)에 T1~T8을 상세히 기록했다.

