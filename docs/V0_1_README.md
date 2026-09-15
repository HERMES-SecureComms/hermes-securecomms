> Archived v0.1 guide. Run the explicit UID mode described in [migration](MIGRATION_V0_1_TO_V0_2.md); the current default is Secure.

# HERMES

> **Press. Authenticate. Communicate.**  
> 누르고, 확인하고, 통신한다.

HERMES는 **Operator-bound Secure PTT** 연구 프로젝트다. 이 저장소는
`PoC v0.1`에만 초점을 맞춘다. 설정된 NFC 태그 UID가 인식되고 사용자가
PTT 버튼을 누르고 있는 동안에만 모의 송신 게이트가 활성화되는지 검증한다.

현재의 UID 비교 방식은 **보안 인증 방식이 아니며**, 사용자의 신원을
증명하는 수단으로 취급해서는 안 된다.

## 프로젝트 목적과 해결하려는 문제

일반적인 현장 통신 장비는 단말을 가지고 있다는 사실만 신뢰하는 경우가 많다.
즉, 단말에 접근할 수 있는 사람이라면 누구나 PTT를 눌러 송신할 수 있다.
HERMES는 통신을 시도하는 순간에 사용자에게 권한이 있는지 추가로 확인하는
구조를 연구한다. 이번 PoC에서는 장갑에 부착한 NFC 태그가 사용자 Credential
역할을 대신한다.

PoC v0.1이 답하려는 질문은 하나뿐이다.

> 설정된 NFC UID가 존재하고 PTT 버튼이 눌린 경우에만 모의 TX 출력이
> 활성화되는가?

## PoC v0.1 범위

이번 버전에 포함되는 항목은 다음과 같다.

- Arduino framework 기반 ESP32-S3 펌웨어
- SPI 방식 PN532 리더와 ISO14443A UID 읽기
- 설정된 UID 비교와 1,500 ms 인식 유예 시간
- 디바운스를 적용한 active-low PTT 버튼 입력
- 모의 `TX_GATE`, 허용 LED, 거부 LED 출력
- 115200 baud 상태 전환 기반 시리얼 로그
- Fail-closed 부팅과 PN532 초기화 실패 후 영구 TX 차단

다음 항목은 명시적으로 범위에서 제외한다: 실제 RF 송신, 실제 무전기 PTT
라인 연결, Falconclaw reverse engineering, Bluetooth, Meshtastic,
OpenMANET, ATAK, ECG/PPG, custom PCB, production cryptography, cloud
backend.

## 시스템 구조

```text
[Hermes Glove]
     |
 [NFC Tag]
     |
     v
 [PN532]
     |
     v
 [ESP32-S3] <--- [PTT Button]
     |
     +----> TX Gate (GPIO 모의 출력만 사용)
     |
     +----> Green / Red LED
```

판정 로직은 fail-closed 방식이다. Credential이 없거나, 등록되지 않았거나,
유효 시간이 끝났거나, 읽을 수 없는 경우에는 `TX_GATE`가 LOW로 유지된다.
PTT를 놓는 즉시 LOW로 돌아간다. 상태와 신뢰 모델은
[아키텍처 문서](ARCHITECTURE.md)에 설명되어 있다.

## 하드웨어 요구사항

- ESP32-S3 development board(기본 PlatformIO 대상은 DevKitC-1)
- SPI를 지원하는 PN532 NFC module
- 장갑에 부착하거나 올려둘 ISO14443A NFC tag/card
- Normally-open momentary push button 1개
- Green LED 1개, Red LED 1개
- LED마다 220–330 Ω 직렬 저항 1개
- Breadboard, jumper wire, USB cable

보드에 따라 사용할 수 있는 핀과 표기가 다르다. 선택한 개발 보드가 설정된
GPIO를 모두 외부로 제공하는지 확인한다.

## 소프트웨어 요구사항

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html) 또는
  PlatformIO IDE extension
- Arduino core for ESP32(PlatformIO가 설치)
- Adafruit PN532 library(`platformio.ini`를 통해 설치)
- 운영체제에서 요구하는 경우 ESP32-S3 보드용 USB serial driver

펌웨어는 Arduino framework를 사용하지만 기준 빌드 구조는 PlatformIO다.
Arduino IDE 사용자는 ESP32 board package와 Adafruit PN532 library를 설치한
뒤 ESP32-S3 sketch를 만든다. `firmware/src`의 파일을 sketch tab으로
복사하면서 `main.cpp`를 `.ino` 기본 탭으로 바꾸고,
`firmware/include`의 헤더도 같은 sketch 디렉터리에 복사한다. 다른
`setup()` 또는 `loop()`를 동시에 컴파일하면 안 된다.

## 저장소 구조

```text
hermes-poc/
├── README.md
├── LICENSE
├── SECURITY.md
├── .gitignore
├── firmware/
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h
│   │   └── credentials.h
│   └── src/
│       ├── main.cpp
│       ├── nfc_auth.h
│       ├── nfc_auth.cpp
│       ├── ptt_controller.h
│       └── ptt_controller.cpp
├── docs/
│   ├── ARCHITECTURE.md
│   ├── WIRING.md
│   ├── TEST_PLAN.md
│   └── ROADMAP.md
└── assets/
    └── README.md
```

## 배선 요약

| 기능 | ESP32-S3 GPIO | 연결 |
|---|---:|---|
| PN532 SCK | 12 | PN532 SCK |
| PN532 MISO | 13 | PN532 MISO |
| PN532 MOSI | 11 | PN532 MOSI |
| PN532 SS/CS | 10 | PN532 SS/SSEL/CS |
| PTT button | 4 | GND로 연결되는 normally-open 버튼(`INPUT_PULLUP`) |
| TX gate | 5 | GPIO test point만 사용, 무전기 연결 금지 |
| Allow LED | 6 | 220–330 Ω 저항 → Green LED → GND |
| Deny LED | 7 | 220–330 Ω 저항 → Red LED → GND |

정확한 PN532 모듈이 지원하는 전압으로 전원을 공급하고 ESP32-S3와 GND를
공유한다. 기준 배선은 3.3 V다. 모듈마다 mode 선택 방식이 다르므로 해당
모듈의 silkscreen 또는 datasheet에 따라 SPI mode로 설정한다. 전원을 넣기
전에 [상세 배선 문서](WIRING.md)를 확인한다.

## 빌드 및 플래시

저장소 루트에서 다음 명령을 실행한다.

```bash
cd firmware
pio run
pio run --target upload
pio device monitor --baud 115200
```

업로드 포트를 자동으로 찾지 못하면 `--upload-port /dev/…`를 추가한다.
다른 ESP32-S3 보드는 `firmware/platformio.ini`의 `board`를 변경한다.
핀 번호는 `firmware/include/config.h`에서 변경한다.

## 허용할 NFC UID 등록 방법

1. `credentials.h`의 예제 UID를 그대로 둔 채 빌드하고 플래시한다.
2. 115200 baud serial monitor를 열고 등록할 테스트 태그를 리더에 댄다.
3. `[HERMES] NFC UID: 04 12 AB CD` 같은 로그에서 UID byte를 복사한다.
4. `firmware/include/credentials.h`의 `AUTHORIZED_UID`를 바꾼다.

   ```cpp
   constexpr uint8_t AUTHORIZED_UID[] = {0x04, 0x12, 0xAB, 0xCD};
   ```

5. 다시 빌드하고 플래시한 뒤 일치하는 태그와 다른 태그를 모두 검증한다.

`credentials.h`에는 예제 식별자만 들어 있다. NFC UID는 공개되고 복제될 수
있는 식별자이지 secret key가 아니다. 실제 암호 키를 저장소에 커밋하거나 이
비교 방식을 secure authentication이라고 표현해서는 안 된다.

## 실행 방법

1. GPIO5를 실제 무전기와 분리한 상태에서 USB 전원을 공급한다.
2. 부팅 로그에서 PN532가 감지되고 `TX_GATE`가 LOW인지 확인한다.
3. 설정된 NFC 태그를 대고 `AUTH SUCCESS` 로그를 확인한다.
4. PTT를 누르는 동안 Green LED와 GPIO5가 HIGH, Red LED가 OFF인지 확인한다.
5. PTT를 놓고 GPIO5와 Green LED가 LOW가 되는지 확인한다.
6. 태그가 없는 경우와 다른 태그도 반복한다. 이때 GPIO5는 LOW를 유지하고
   PTT를 누르는 동안 Red LED가 켜져야 한다.

기본값 `AUTH_GRACE_PERIOD_MS = 1500`은 breadboard 환경에서 NFC가
순간적으로 누락되는 현상을 완화하기 위한 값이다. **실제 제품의 보안 session
timeout이 아니다.** 마지막 정상 태그 인식 후 이 시간이 지나면 권한이
만료되고, PTT를 계속 누르고 있더라도 TX가 차단된다.

## 시리얼 로그 예시

로그는 loop마다 반복되지 않고 상태가 바뀔 때 출력된다. PTT와 TX 이벤트에는
향후 latency 측정을 위한 `millis()` 기반 `t_ms`가 포함된다.

```text
[HERMES] Booting...
[HERMES] TX DISABLED
[HERMES] PN532 detected
[HERMES] PN532 firmware: 1.6
[HERMES] Waiting for credential
[HERMES] System ready
[HERMES] NFC UID: 04 12 AB CD
[HERMES] AUTH SUCCESS
[HERMES] PTT PRESSED t_ms=4832
[HERMES] TX ENABLED t_ms=4832
[HERMES] PTT RELEASED t_ms=6144
[HERMES] TX DISABLED t_ms=6144
```

PTT 입력에서 TX 활성화까지의 측정 결과는 아직 **TBD**다. 측정 장비와 반복
횟수를 정의해 실측하기 전에는 성능 값을 주장하지 않는다.

## 데모 시나리오

| 시나리오 | 기대 동작 |
|---|---|
| 등록 태그가 있고 PTT를 누름 | Green ON, Red OFF, `TX_GATE` HIGH |
| 다른 태그가 있고 PTT를 누름 | Green OFF, Red ON, `TX_GATE` LOW |
| 태그 없이 PTT를 누름 | Green OFF, Red ON, `TX_GATE` LOW |
| PTT를 놓음 | Green OFF, `TX_GATE` LOW |
| TX 중 등록 태그를 제거함 | 1,500 ms 유예 시간 후 TX 차단 |
| 만료 전에 등록 태그를 다시 인식시킴 | 읽기 결과에 따라 권한 유지/복구 |
| ESP32-S3를 재부팅함 | 초기화가 끝날 때까지 TX 차단 |
| 부팅 중 PN532를 감지하지 못함 | Red ON, 재부팅 전까지 TX 영구 차단 |

상세 수동 검증 절차는 [TEST_PLAN.md](TEST_PLAN.md)에 있다.

## 보안 한계

PoC v0.1은 NFC UID whitelist 비교를 사용한다. UID 기반 식별은 **보안 인증
방식이 아니다.** 호환되는 태그 UID는 관찰하거나 복제할 수 있다. 현재
구현에는 challenge-response, replay resistance, secure element, 물리적
tampering 방어, credential theft 방어, 안전한 key storage가 없다. 로그의
`AUTH SUCCESS`는 오직 “읽은 UID가 로컬 whitelist와 일치함”을 뜻한다.

이는 PoC v0.1에서 의도한 한계이며 production 보안 보장이 아니다. 후속
단계에서는 UID 비교를 Secure NFC, cryptographic challenge-response,
session management, secure element 방식으로 교체할 예정이다. 테스트 또는
재사용 전에 [SECURITY.md](SECURITY_V0_1.md)를 읽는다.

## 로드맵

- **v0.1:** UID-based NFC, PTT gate simulation, LED
- **v0.2:** Secure NFC, cryptographic challenge-response, session management
- **v0.3:** 실제 radio/headset 전기 특성 측정과 절연 interface 설계
- **v0.4:** Device binding, remote revoke, Meshtastic control-plane 연구
- **v1.0:** Custom PCB, hardened enclosure, secure element, field testing

OpenMANET, ATAK, Bluetooth는 향후 확장 가능성일 뿐이며 현재 구현에는
포함되지 않는다. 상세 내용은 [ROADMAP.md](ROADMAP.md)에 있다.

## 안전 경고

> **ESP32 GPIO를 실제 무전기의 PTT 라인에 직접 연결하지 말 것.**
>
> **Do not connect an ESP32 GPIO directly to a real radio PTT line.**

GPIO5는 LED, logic analyzer 또는 고임피던스 multimeter를 위한 logic-level
PoC 출력일 뿐이다. 실제 무전기는 전압, 전류, 극성, 접지, keying 방식이
호환되지 않을 수 있다. 후속 interface를 만들기 전에 먼저 무전기 특성을
측정하고, 그 결과에 맞는 optocoupler, transistor, MOSFET 또는 analog
switch 회로와 독립적인 hardware default-off 상태를 설계해야 한다.

이 저장소는 실제 RF를 송신하지 않으며 실제 무전기를 제어하지 않는다.

