# Threat Model

## 범위와 자산

보호 대상은 논리적인 **TX permission**과 오래된 세션을 계속 허용하지 않는
정책이다. Simulator는 RF, NFC, GPIO, 실제 사용자 신원을 검증하지 않는다.
Python runtime과 테스트 제어자는 신뢰하고, 시뮬레이션 속 Credential 입력,
장비 탈취, 분리, revoke, 고장을 정책 검증 대상으로 본다.

| Threat | 시나리오 | v0.1이 검증하는 대응 | 해결하지 못하는 부분 |
|---|---|---|---|
| T1 Radio/PTT theft | 공격자가 단말만 탈취 | Credential 부재/만료 상태에서 PTT 거부 | grace 내 잔여 권한, 실제 장비 조작/배선 우회 |
| T2 Unknown credential | 미등록 ID 제시 | AUTH FAILED와 DENY_UNKNOWN_CREDENTIAL; 기존 권한 즉시 폐기 | ID 자체의 진위나 암호학적 증명 |
| T3 Credential removal | TX 중 장갑 제거 | grace 종료와 함께 held PTT도 자동 차단 | RF 간섭과 실제 센서 dropout 구분, grace 내 사용 |
| T4 Credential itself stolen | 단말과 정상 Credential을 함께 탈취 | 철회되었다면 거부 | **철회 전에는 원 사용자와 공격자를 구분하지 못함** |
| T5 Device revoked | 인증된 장치에 revoke 정책 적용 | 즉시 권한 무효화, 정상 Credential로도 거부 | 실제 revoke 배포/전달 지연, 명령 권한 검증 |
| T6 Controller malfunction | controller fault | unhealthy에서 항상 deny, recover 후 재인증 | 임의 메모리 변조, CPU hang, 전기적 출력 고착 |
| T7 Stale authentication | 분리 후 오래된 인증 재사용 | 정확한 deadline 경계, 중복 detach 연장 금지, restore/recover 후 세션 자동 복구 금지 | 실제 session protocol, 동시성, 실시간 스케줄링 |
| T8 Credential cloning | 등록 ID 복제/에뮬레이션 | 철회된 ID는 거부 | 복제 ID와 진짜 Credential 구분; replay 방어 |

## UID 및 Credential 한계

현재 hardware PoC는 UID whitelist 기반이다. UID는 불신 식별자이며 복제할 수
있다. Simulator의 `credential accepted` 또는 `AUTH SUCCESS`는 설정된 예제
등록 항목을 수락했다는 뜻이다. 생체 인증, 강한 인증, 복제 불가능성을 제공하지 않는다.

**Credential 자체를 공격자가 함께 탈취한 경우**, v0.1만으로 원 사용자와
공격자를 구분할 수 없다. operator_id는 등록 메타데이터이지 현 사용자에 대한
liveness나 두 번째 인증 요소가 아니다.

Secure NFC, challenge-response, secure element는 향후 강화 방향이다.
현재 코드에는 cryptographic authentication, nonce, secret key, secure boot,
secure element access가 없다.

## 제어 명령과 신뢰 가정

`device restore`, `system recover`, configuration 변경은 시뮬레이션을 진행하기
위한 로컬 제어 기능이다. production 환경에서 restore를 단순 로컬 명령으로
수행해서는 안 되며, 관리 권한·정책 배포·감사·영속적인 철회 상태를 설계해야 한다.

같은 시뮬레이션 내 reboot는 철회를 보존하지만, 프로세스를 재시작하면
configuration에서 새 상태를 만든다. 이벤트와 철회 history는 메모리에만 있다.
이를 보안 감사 보존이나 공격자에 대한 지속적인 enforcement로 간주하지 않는다.

## 의도한 노출과 비목표

1,500 ms grace는 hardware PoC와 맞추기 위한 parameter이며 실제 제품 보안
timeout이 아니다. 이 구간은 Credential이 없더라도 TX를 허용할 수 있다.
PTT level 정책 때문에 복구 후 정상 Credential을 재제시하면 held PTT가 다시
활성화될 수 있다. 두 동작 모두 테스트와 문서에 노출된 정책 선택이다.

실제 radio/SDR/RF control, Bluetooth, NFC reader, Meshtastic transmission,
OpenMANET/ATAK networking, backend, database, cloud, production deployment는 없다.
시뮬레이터의 테스트 통과는 실제 전기적 fail-closed 동작의 입증이 아니다.

