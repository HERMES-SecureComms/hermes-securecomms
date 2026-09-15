# HERMES v0.2 — Security PoC

> Press. Authenticate. Communicate.

HERMES v0.2는 credential별 비밀키의 보유를 HMAC-SHA256으로 검증하고,
짧은 session과 PTT 장치에 인증 결과를 바인딩하는 보안 PoC입니다.
TX는 GPIO/LED 모의 출력입니다.

```text
Secure Credential → Challenge-Response → Counter / Replay Check
→ Signed Revocation / Anti-Rollback → Device Binding
→ Short-lived Session → Policy Engine → PTT Allow / Deny
```

## 구현 범위

- 매 인증마다 CSPRNG nonce와 일회용 challenge, credential별 단조 counter
- Ed25519 서명 폐기 목록, policy/CRL/credential metadata 버전 rollback 차단
- ACTIVE/REVOKED/DISABLED credential, 선택 가능한 장치 allowlist
- credential + device에 묶인 session, 만료 시 PTT를 계속 눌러도 TX 차단
- NORMAL/DEGRADED/REVOKED 정책과 typed decision, 기본 fail-closed
- 시스템/저장소/인증 오류 시 session 제거, 복구 후 반드시 재인증
- 표준 암호 라이브러리, KeyStore/transport/정책/하드웨어 분리
- Python 시뮬레이터, C++ 보안 코어, 9개 golden scenario, 구조화 trace

**SoftwareKeyStore는 PoC/test 전용이며 production 사용을 금지합니다.**
실제 production secret은 포함하지 않습니다.

## 실행 모드와 현재 하드웨어 경계

| 모드 | 동작 |
|---|---|
| 기본 ESP32 Secure | 키/신뢰 anchor 미설정 상태에서 TX 차단. UID로 허용하지 않음 |
| `secure-mock` | test fixture 키와 software credential로 전체 보안 흐름 실행 |
| `uid-poc` | 명시적인 v0.1 UID 회귀 모드, 보안 인증 아님 |
| 기본 Python CLI | v0.2 보안 시뮬레이터, 실행마다 별도 임시 credential 키 생성 |

PN532 초기화/UID 읽기는 유지했습니다. 실제 secure NFC card의 APDU 드라이버와
Secure Element 드라이버는 아직 선택되지 않았으므로 해당 adapter는 명시적으로
실패합니다. 기본 펌웨어가 실제 secure NFC 인증을 수행한다고 주장하지 않습니다.
전체 상태 전이는 hardware 없이 시뮬레이터/native test에서 검증할 수 있습니다.

## Firmware build

PlatformIO Core 설치 후:

```sh
cd firmware
pio run -e esp32-s3-devkitc-1 -e uid-poc -e secure-mock
bash test/native/run.sh
```

Native test는 C++17 compiler와 OpenSSL 개발 헤더가 필요합니다. Arduino core는
ESP32-S3/PlatformIO espressif32 6.9.0, PN532 1.3.4, Arduino Crypto 0.4.0을 사용합니다.
`secure-mock`의 serial command는 `a` 인증, `d` 제거, `f` 장애, `r` 복구입니다.
PTT/LED/TX 핀은 기존과 같습니다. [배선 문서](docs/WIRING.md)를 참고하세요.
기본 timeout 5000ms는 실험 설정값이며 제품 스펙이나 실측 성능이 아닙니다.

## Simulator와 보안 검증

이 workspace의 `hermes-simulator` 디렉터리에서:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/python -m pytest -q
.venv/bin/python tools/validate_security.py
.venv/bin/python -m hermes_simulator.cli run scenarios/secure_normal_operation.json
.venv/bin/python -m hermes_simulator.cli run scenarios/replay_attack.json
.venv/bin/python -m hermes_simulator.cli run scenarios/credential_revoked.json
.venv/bin/python -m hermes_simulator.cli run scenarios/session_expired_during_tx.json
.venv/bin/python -m hermes_simulator.cli run scenarios/revocation_list_rollback.json
```

Python 3.12+를 사용합니다. JSON action shell은 `python -m hermes_simulator.cli shell`입니다.
예: `{"action":"authenticate","credential_id":"GLOVE-001"}`, `{"action":"ptt_press"}`.
기존 UID CLI는 `python -m hermes_simulator.legacy_cli shell`로 명시해서 실행합니다.
기존 5개 시나리오는 `scenarios/v0_1/`, UID 설정은 `config.uid-poc.example.json`입니다.

## 문서

- [Architecture](docs/V0_2_ARCHITECTURE.md), [Authentication protocol](docs/AUTH_PROTOCOL.md)
- [Key management](docs/KEY_MANAGEMENT.md), [Session model](docs/SESSION_MODEL.md)
- [Revocation](docs/REVOCATION.md), [Threat model](docs/THREAT_MODEL_V0_2.md)
- [Security test plan](docs/SECURITY_TEST_PLAN.md), [Migration](docs/MIGRATION_V0_1_TO_V0_2.md)
- [실제 검증 결과와 변경 파일](docs/VALIDATION_REPORT.md), [기존 v0.1 README](docs/V0_1_README.md)

## 한계와 다음 단계

장갑과 정품 credential 동시 탈취, 사용자 강압, 물리적 변조/키 추출, firmware
수정, 전체 저장소 snapshot rollback, live relay를 완전히 해결하지 못합니다.
Secure Boot/Flash Encryption은 조사·문서화했으며 활성화하지 않았습니다.
실제 secure-card/secure-element adapter, 보호된 provisioning/저장소, 전원 차단
시험과 장비 성능 실측이 다음 단계입니다. 모든 하드웨어 latency 값은 TBD입니다.

실제 RF 송신, 무전기/헤드셋 제어, 주파수 설정, cloud/dashboard, Bluetooth,
Meshtastic/OpenMANET/ATAK, ECG/PPG/UWB, custom PCB는 구현하지 않습니다.
**ESP32 GPIO를 실제 무전기 PTT 라인에 연결하지 마세요.**
