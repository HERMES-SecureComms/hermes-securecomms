# HERMES v0.2 authentication protocol

## Actors and messages

Actors: **Hermes PTT** (verifier) and **Hermes Credential** (key holder).
The credential ID, operator ID and key ID are identifiers, never secrets.
This is proof of key possession by equipment; it does not authenticate a human.

Message 1, Challenge:

| Field | Encoding / meaning |
|---|---|
| protocol_version | unsigned big-endian 16-bit integer, exactly 2 |
| ptt_device_id / device_id | `text(device_id)` |
| nonce | 32 fresh CSPRNG bytes per attempt |
| policy_version | unsigned big-endian 64-bit, positive |
| session_context | 16 additional fresh CSPRNG bytes |

Message 2, Credential Response:

| Field | Encoding / meaning |
|---|---|
| credential_id | `text(credential_id)` |
| key_id | `text(key_id)` |
| counter | unsigned big-endian 64-bit integer, 1 through 2^64-1 |
| response_mac / MAC | full 32-byte HMAC-SHA256 output |

`text(x)` is a 16-bit big-endian byte length followed by ASCII bytes. IDs contain
1–64 characters from `[A-Za-z0-9_.-]`. No implicit separators, native integer
layout, JSON reserialization, or Unicode normalization enter the MAC.

The exact authenticated byte string, in order, is:

```text
ASCII("HERMES-AUTH-v2") || 0x00 ||
u16be(2) || text(ptt_device_id) || nonce[32] ||
u64be(counter) || u64be(policy_version) || session_context[16] ||
text(credential_id) || text(key_id)
```

`response_mac = HMAC-SHA256(CredentialKey, authenticated_bytes)`.
The session context and both response IDs are authenticated in addition to the
required device/nonce/counter/policy fields. The operator identity comes only
from trusted versioned credential metadata, not from the response.

## Verification order

First require a healthy verifier, active device and a live pending challenge.
Consume that challenge on **every** submitted response, including failures.
Then validate response shape and apply:

1. Credential exists.
2. Credential is ACTIVE and not on the accepted revocation list.
3. Key ID equals current credential metadata.
4. Counter strictly exceeds that credential's last accepted counter.
5. Configured device binding matches.
6. Standard HMAC verification uses constant-time MAC comparison.
7. Commit the accepted counter before making a session available.
8. Generate a random session ID and bind it to credential, operator, device,
   policy and a fixed deadline.

The Python implementation uses `hmac.digest`/`hmac.compare_digest`; ESP32 uses
mbedTLS HMAC-SHA256, and host C++ uses OpenSSL. No custom cryptographic primitive
is implemented. The wire protocol is a PoC composition, not a standardized or
formally verified mutual-authentication protocol.

## Replay, freshness and time

Response A fails for challenge B because B has a different nonce and session
context. A previously accepted response also fails the strict counter check:
equal counter returns `DENY_REPLAY` / `REPLAY_OR_INVALID_MAC`; a lower counter
returns `DENY_COUNTER_ROLLBACK` / `COUNTER_ROLLBACK`. A captured but not yet
accepted response from another challenge fails the MAC with
`REPLAY_OR_INVALID_MAC`. A valid response cannot be submitted twice.

Challenge validity is `now < created + challenge_timeout_ms`; equality expires.
A new challenge invalidates the previous challenge and active session.
`challenge_timeout_ms=2000` is the default experiment parameter, configurable.
Clock reversal or arithmetic exhaustion fails closed. A credential increments
and durably reserves its counter before releasing a response. Lost responses
may skip counters; wraparound is forbidden. Counter exhaustion needs controlled
reprovisioning, not an automatic reset. See [key management](KEY_MANAGEMENT.md).

## Entropy on ESP32-S3

Python uses `secrets.token_bytes` and `secrets.token_hex`, with no seeded PRNG.
ESP32 calls `bootloader_random_enable()`, then `esp_fill_random()`, then
`bootloader_random_disable()` for each nonce/context/token generation. This
activates the internal non-RF entropy source. The firmware does not initialize
ADC, Wi-Fi or Bluetooth. That precondition must be revisited if peripherals are
added. Espressif documents that `esp_fill_random` needs an enabled entropy source;
bootloader entropy alone is insufficient for a continuing true-random stream.
[ESP-IDF 4.4 RNG documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/random.html)

The fixed bytes in `tests/fixtures/protocol_vector.json` are interoperability
fixtures only. Runtime challenges never use those bytes or a fixed random seed.
Live relay, proximity measurement, mutual PTT authentication and credential
secure-hardware side channels are outside this protocol's guarantees.
