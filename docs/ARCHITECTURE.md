> v0.1 reference retained for regression. Current security design: [v0.2 architecture](V0_2_ARCHITECTURE.md) and [security tests](SECURITY_TEST_PLAN.md).

# Architecture

## Objective

HERMES PoC v0.1 proves a narrow physical interaction: a configured NFC UID must
be recognized before pressing PTT can assert a simulated TX gate. It does not
provide cryptographic operator authentication or radio integration.

```text
Untrusted physical side                         PoC controller

[Glove + NFC tag] --RF field--> [PN532] --SPI--> [NFC UID comparison]
                                                       |
[PTT button] -------------------------------> [PTT controller]
                                                       |
                                +----------------------+-------+
                                |                              |
                         [TX_GATE GPIO]                  [status LEDs]
```

## Components

### Hermes Glove Credential

An ISO14443A tag is attached to a glove for the demonstration. Its UID is a
cloneable, untrusted identifier. “Credential” describes its role in the demo,
not a security property.

### PN532 NFC Reader

The PN532 communicates with the ESP32-S3 over SPI. Startup reads its firmware
version and configures SAM operation. If either operation fails, the controller
latches a permanent fault and will not enable TX until the board is reset and a
successful initialization occurs.

### ESP32-S3

The ESP32-S3 runs the Arduino firmware, polls the reader, debounces PTT, and
drives the three outputs. Pin, grace-period, polling, debounce, and serial
settings live in `config.h`.

### Authentication Module

`NfcAuth` reads and prints ISO14443A UIDs and compares them byte-for-byte with
the example whitelist entry in `credentials.h`. A matching observation sets the
PoC authorization state. Missed reads retain that state for at most 1,500 ms;
an explicitly different UID revokes it immediately. This is UID matching, not
secure authentication.

### PTT Controller

`PttController` interprets `LOW` as pressed after a 30 ms debounce. Its TX
decision is the conjunction:

```text
TX_ENABLE = system_ready AND no_latched_fault AND uid_authorized AND ptt_pressed
```

Any false input disables TX. State-transition logs avoid flooding the serial
port. Timestamps on PTT/TX events allow future latency analysis without claiming
an unmeasured result.

### TX Gate

GPIO5 is a protected logical output within the PoC boundary. It is driven HIGH
only when `TX_ENABLE` is true and is reasserted LOW on every disabled controller
update. It is only a simulation/test point and must not be connected to a radio.

### Status LEDs

- Green indicates that simulated TX is enabled.
- Red indicates a denied PTT attempt or a latched PN532 startup fault.
- Both are off during normal idle operation.

## State flow

```text
BOOT --force TX LOW--> PN532 CHECK
                           | success
                           v
                      IDLE / DENIED
                           |
                    matching UID seen
                           v
                       AUTHORIZED
                           | PTT pressed
                           v
                      TX ENABLED

Any PTT release ---------------------------> TX DISABLED
UID absent >= grace period ----------------> TX DISABLED
Different UID -----------------------------> TX DISABLED
PN532 startup failure --> LATCHED FAULT ----> TX DISABLED
```

If a credential expires while PTT remains held, the controller drops TX on that
loop and lights red. If a matching UID returns while PTT is still held, the
logical conjunction becomes true again; test procedures should explicitly
observe and decide whether this is appropriate for later session semantics.

## Trust boundaries

| Item | PoC treatment | Rationale |
|---|---|---|
| NFC UID | **Untrusted identifier** | It can be observed or cloned and has no proof of possession |
| PN532-to-ESP32 data | Unauthenticated input | SPI wiring is physically exposed and messages have no integrity protection |
| ESP32 configuration/firmware | **Trusted for this PoC** | UID and behavior are compiled locally; secure boot is not configured |
| PTT button | Untrusted physical input | Debounced, but not protected against wiring manipulation |
| `TX_GATE` | **Protected output within the PoC** | Only the controller writes it; it is not a production safety boundary |

The trust assumptions are intentionally weak and suitable only for functional
validation. Secure NFC challenge-response, protected keys, secure boot, hardware
tamper measures, and an electrically isolated default-off output belong to later
phases.

## Fail-closed behavior

- Output latches are set LOW before the pins enter output mode in `setup()`.
- No recognized UID means authorization is false.
- A different observed UID revokes an existing authorization immediately.
- Credential loss expires at the configured grace deadline.
- PTT release disables TX independently of NFC state.
- PN532 initialization failure is latched; the firmware does not retry into an
  accidentally permissive state.
- Unknown or contradictory conditions make the TX decision false.

Reset-time electrical behavior still requires an external pull-down/default-off
interface in any future real device. Software behavior is not a substitute for
that hardware control.

