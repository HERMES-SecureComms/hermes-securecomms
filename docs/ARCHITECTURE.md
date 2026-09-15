> v0.1 reference retained for regression. Current security design: [v0.2 architecture](V0_2_ARCHITECTURE.md) and [security tests](SECURITY_TEST_PLAN.md).

# Architecture

HERMES Simulator는 **Software Reference Model**이다. 입력 action과 가상 시간에
대해 상태와 TX decision을 결정적으로 계산하며, 하드웨어에 접근하지 않는다.

```text
cli.py / JSON scenario
         |
   simulator.py --------------------> clock.py
         |                        VirtualClock / FakeClock
   state_machine.py
         |       |
         |       +----> events.py (append-only event values)
         v
 models.PolicyContext (immutable snapshot)
         |
     policy.evaluate
         |
     TxDecision enum
```

## 핵심 파일의 역할

| 파일 | 책임 |
|---|---|
| `__init__.py` | 패키지 및 버전 |
| `models.py` | frozen dataclass Credential/Device/Configuration/PolicyContext, 상태와 decision enum |
| `clock.py` | Clock protocol, 수동 전진 VirtualClock, 테스트용 FakeClock |
| `policy.py` | 문맥을 받아 이유가 있는 TX decision을 반환하는 순수 함수 |
| `state_machine.py` | 인증·유예·PTT·철회·장애 전이, 세션 무효화, TX 재평가, 이벤트 생성 |
| `events.py` | EventType과 변경 불가능한 Event 값 |
| `simulator.py` | 외부 action API, 설정 검증, status snapshot, scenario 검증·실행·비교 |
| `cli.py` | argparse/shlex 명령 파싱, shell, 읽기 쉬운 로그, JSON event 출력 |

상태 머신은 출력하지 않고, policy는 기록·시간 조회·상태 변경을 하지 않는다.
상태 변경은 단일 스레드의 명령 경계에서 처리한다. 공개 API는
`Simulator.execute(action, **arguments)`와 `Simulator.status()`다.

## 데이터와 인증 수명

등록 Credential은 ID, operator ID, authorized, revoked를 갖는다. Device는 ID,
revoked, system_healthy를 갖는다. 상태 머신은 부착된 ID와 마지막 세션 ID를
구분한다. 분리 후 grace에서도 세션 Credential로 정책을 평가하고 로그에
operator를 남길 수 있기 때문이다.

`session_valid`는 정상 attach에서만 설정된다. detach는 정상 세션에 대해서만
grace를 시작한다. unknown replacement, 철회, 장애, 만료는 이를 무효화한다.
복구 명령은 healthy/철회 flag만 복구하며 인증 성공을 생성하지 않는다.

Status의 `credential_id`는 현재 부착 ID다. 분리 후에는 null이고, `operator_id`는
마지막 세션의 감사용 식별자가 남을 수 있다. Operator 문자열의 존재는 권한을
의미하지 않으며, 권한은 `session_valid`, `auth_state`, `decision`으로 확인한다.

## 시간과 이벤트

Clock은 milliseconds를 반환하고 음수가 아닌 정수만 전진한다. 시간은
`time advance`에서만 흐른다. 명령 완료 시 만료를 반영하고 TX를 재평가하므로
PTT를 계속 누르고 있어도 만료 명령이 끝나기 전에 TX는 비활성화된다.

외부 테스트가 FakeClock을 직접 전진해도 다음 status/decision 조회에서 상태를
refresh한다. Policy 자체도 grace deadline을 확인하므로 오래된 snapshot에
새 시간을 넣었을 때 만료 세션을 허용하지 않는다.

이벤트의 `timestamp`는 관측/처리 시점의 가상 ms다. 0에서 1600으로 한 번에
진행했다면 deadline은 1500이지만 `AUTH_EXPIRED`는 1600에 기록된다. 중간 시간의
실제 TX 파형이나 latency를 시뮬레이션했다고 해석하면 안 된다.

Event 필드는 timestamp, event_type, device_id, credential_id, operator_id,
reason이며 일부 이벤트에 milliseconds가 추가된다. `events`는 JSON lines로
내보낸다. history는 tuple로 노출되고 이벤트 값은 frozen이다. 반복 PTT 입력이나
상태 조회는 동일한 TX 전환 이벤트를 중복 생성하지 않는다.

재부팅에서도 audit history, 가상 시간, 장치/credential 철회 기록은 유지한다.
프로세스를 종료하면 모두 소멸한다. 저장소나 외부 revoke service는 없다.

## 실제 Firmware와 비교

| 항목 | Simulator | ESP32-S3 + PN532 hardware PoC |
|---|---|---|
| Credential 입력 | `attach_credential`/`detach_credential` | ISO14443A UID 읽기와 read dropout |
| 인증 | 예제 Credential registry 조회 | 설정된 UID whitelist 비교 |
| 정책 | Credential → State Machine → Policy Engine → TX Decision | PN532 → ESP32 Policy Logic → TX_GATE |
| Grace 시작 | 명시적 detach 시각 | 마지막 정상 UID 읽기 시각 기준 |
| 시간 | 수동 virtual milliseconds | 보드 timer와 polling 주기 |
| PTT | 논리적 press/release | GPIO 및 debounce |
| TX | enum/boolean 결과 | 모의 GPIO/LED 출력 |
| 철회·복구 | 명시적 모델 구현 | 기존 v0.1 firmware에 전부 구현되었다고 가정하지 않음 |

동일한 기본 보안 정책을 목표로 하지만 전기적·시간적 동등성은 아직 검증하지
않았다. 후속 비교에서는 last-seen/detach 정의, polling 지연, 버튼 debounce를
명시적으로 정규화하고 사건별 상태·decision·TX disable 순서를 비교해야 한다.

## 신뢰 경계

시나리오/CLI가 묘사하는 Credential은 불신 입력이다. 반면 로컬 테스트 제어자,
configuration, Python runtime은 신뢰한다. Python 내부 상태를 직접 바꾸는
공격자에 대한 tamper 방어는 없으며, immutable 값은 코드 실수를 줄이기 위한
구조다. 실제 보안 경계나 tamper-proof 감사 로그로 간주하지 않는다.

