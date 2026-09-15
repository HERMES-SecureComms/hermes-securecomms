# HERMES v0.2 implementation and validation report

Verified at 2026-09-15T00:34:03.624334+00:00. No Git commands, flashing, eFuse changes or actual radio/RF
operations were performed. All requested software/build checks below passed.
Physical secure-card authentication remains an explicitly unimplemented hardware
adapter boundary; the full Security PoC runs in Python/native and secure-mock.

## 1. Existing repository analysis

`hermes-poc` held the ESP32-S3/Arduino PN532 UID gate. `hermes-simulator` contained
an identical firmware copy and a Python v0.1 package with five scenarios and
97 tests. Authentication, hardware output and policy were partly separated,
but UID presence was the authentication decision. Initial v0.1 build passed;
all 97 legacy tests passed before secure additions and still pass in the final suite.

## 2. Changed files

[CHANGED_FILES.md](CHANGED_FILES.md) lists every added/changed/relocated authored
file using before/after SHA-256 inventory rather than Git. Build/cache files are
excluded. Both firmware source trees match byte-for-byte (21 authored files).
Legacy models/policy/state machine/clock/events and five moved scenarios match
their before-change hashes. The copied legacy facade is unchanged; legacy CLI
only redirects its facade import. Test expected outcomes were preserved.

## 3. New architecture

`CredentialTransport → Authentication Engine → durable counter/replay checks →
revocation/binding → Session → pure Policy Engine → PttController GPIO/LED`.
HMAC authenticates version/device/nonce/counter/policy/context/credential/key IDs.
Ed25519 authenticates CRLs. Sessions are device/credential-bound and never resumed
from an external ID. Expiry is inclusive at the deadline. See
[V0_2_ARCHITECTURE](V0_2_ARCHITECTURE.md) and [AUTH_PROTOCOL](AUTH_PROTOCOL.md).

## 4–6. Builds, simulator and security tests

| Actual command / environment | Result | Evidence |
|---|---|---|
| `python -m pytest -q` | **173 passed**: 97 legacy + 76 security cases | [pytest log](validation/python-tests.log) |
| `python tools/validate_security.py` | **1,308 C++/GPIO checks; 9 parity comparisons PASS** | [native/parity log](validation/native-parity.log) |
| `pio run -e esp32-s3-devkitc-1 -e uid-poc -e secure-mock`, hermes-poc | **3/3 builds SUCCESS** | [firmware log](validation/firmware-poc-build.log) |
| Same build command, hermes-simulator | **3/3 builds SUCCESS** | [mirror build log](validation/firmware-simulator-build.log) |
| `pip check` | No broken requirements | Python environment checked after editable installation |

Tools used: Python 3.14.6, pytest 9.1.1, cryptography 50.0.1, PlatformIO Core
6.2.0, GCC C++ 15.3.0, host OpenSSL 3.6.3. Firmware platform is espressif32 6.9.0,
Arduino ESP32 2.0.17, PN532 1.3.4, Arduino Crypto 0.4.0. Native tests use
AddressSanitizer/UndefinedBehaviorSanitizer and compile warnings as errors.

The first secure build exposed Arduino's `DISABLED` macro colliding with enum
names. C++ status/binding enumerators were renamed to avoid the collision;
all builds were then rerun successfully. PlatformIO's bundled older esptool
emits a Python 3.14 SyntaxWarning; this is not a project compile/test failure.

## 7–10. Required scenario executions

All nine JSON scenarios were also run through separate real CLI processes with
exit code 0. [Scenario index](validation/scenarios.json) and individual logs:

| Scenario | Observed security result | Trace |
|---|---|---|
| secure_normal_operation | HMAC session then ALLOW_TX; release disables | [log](validation/secure_normal_operation.log) |
| replay_attack | old response on new challenge → DENY_REPLAY / REPLAY_OR_INVALID_MAC | [log](validation/replay_attack.log) |
| counter_rollback | accepted 10, received 9 → DENY_COUNTER_ROLLBACK | [log](validation/counter_rollback.log) |
| credential_revoked | active TX disabled by valid signed CRL; later auth denied | [log](validation/credential_revoked.log) |
| device_binding_failure | HPTT-099 → DEVICE_BINDING_MISMATCH | [log](validation/device_binding_failure.log) |
| session_expired_during_tx | 4999 valid; 5000/5001 expired with PTT held | [log](validation/session_expired_during_tx.log) |
| revocation_list_rollback | accepted v10; signed v9 rejected, v10 retained | [log](validation/revocation_list_rollback.log) |
| tampered_revocation_list | modified signed payload rejected; session removed | [log](validation/tampered_revocation_list.log) |
| system_fault_during_session | TX off; recovery requires explicit new auth | [log](validation/system_fault_during_session.log) |

## 11–12. Overall pass and v0.1 regression

**All executed checks PASS.** No skipped tests or deleted/relaxed assertions were
used to obtain this result. Explicit legacy routing preserves original behavior;
the secure default intentionally replaces UID-only authorization. Native GPIO
tests cover boot-low, debounce, release, auth loss, latched faults and secure
expiry. The hardware wiring/PN532 startup path builds successfully. No physical
board was connected, so real electrical/NFC regression remains unmeasured.

## 13. Current security limitations

Default hardware firmware is unprovisioned and TX-denying. PN532 UID discovery
is implemented; a selected secure-card APDU/mutual-auth driver is not.
SecureElementKeyStore is an error-returning adapter stub. `secure-mock` is the
executable firmware demonstration and uses published test keys with non-durable
memory stores. Production credentials/keys are absent.

Software keys, physical state replacement, firmware modification, glove plus
genuine credential theft, user coercion, advanced side channels/extraction, live
relay, radio firmware compromise and RF-network compromise are not solved.
Secure Boot/Flash Encryption were researched and documented, not enabled.
Anti-rollback checks protect updates against trusted saved state; complete old
flash/disk snapshot restoration is not hardware-protected. CRL freshness while
offline and remote signed policy/metadata distribution are outside the PoC.

## 14. Remaining TODO (hardware / later hardening)

1. Select secure NFC hardware, implement/measure its transport adapter and real
   secure-element KeyStore; provision device identity, trust anchor and keys.
2. Secure Boot V2, Flash/NVS Encryption, protected provisioning/key rotation,
   hardware anti-rollback and authenticated update distribution.
3. Real NVS power-cut/wear tests, bounded NFC scheduling/removal detection,
   hardware default-off/tamper design and on-device crypto-vector validation.
4. Measure the six hardware latency metrics in SECURITY_TEST_PLAN. All remain
   **TBD**, with no invented performance values.

## Requirement-by-requirement completion audit

The scope here is the requested Security PoC. “Hardware boundary” explicitly
identifies the allowed abstraction/test implementation and is not a claim that
real secure NFC, a production secure element or hardened firmware is deployed.

| Request § | Current evidence |
|---:|---|
| 1 | Python/C++ secure engine and default secure entrypoints; normal golden scenario |
| 2 | Standard HMAC/Ed25519/HKDF/library constant-time comparison; no UID secure allow |
| 3 | Credential ID/operator/key/status/counter/binding metadata in both typed models |
| 4 | KEY_MANAGEMENT hierarchy/HKDF helper; separated fixture keys; local-secret ignore rules |
| 5 | Exact transcript and consumed challenge implemented; cross-language vector verification |
| 6 | Python secrets; ESP32 entropy enable/fill/disable; nonce/context uniqueness tests and cited RNG research |
| 7 | Captured response/new challenge, accepted/unaccepted replay, equal/lower counter tests and power-loss contract |
| 8 | Policy/CRL/metadata version floors; stale/equal update tests and persisted state reopen |
| 9 | Configurable disabled/allowlist binding; mismatch and empty-list tests |
| 10 | Random local session handle + credential/operator/device/timestamps/policy; all four session states |
| 11 | Held-PTT expiry tests in Python and actual C++ PttController with fake GPIO |
| 12 | ACTIVE/REVOKED/DISABLED status gates; revoked credential denied despite valid key possession |
| 13 | Ed25519 CRL version/issued_at/entries/reason; signature-before-version implementation |
| 14 | Valid/invalid/modified/old signed lists plus revoked active session tests |
| 15 | Pure typed SecurityContext/evaluate and decision enums, independent of hardware |
| 16 | NORMAL/DEGRADED/REVOKED tests; emergency requires explicit policy and never drives general TX |
| 17 | Reader/keystore/MAC/unknown/revoked/expiry/binding/counter/policy/signature/storage/system fault tests |
| 18 | Fault clears session/challenge; recovery reloads state and requires new authentication |
| 19 | Cited Secure Boot/Flash Encryption research; T12 firmware bypass and future hardening documented |
| 20 | KeyStore, SoftwareKeyStore and error-returning SecureElementKeyStore; production prohibition in README |
| 21 | PN532 initialization/UID retained behind transport; engine independent of PN532; secure-card adapter boundary explicit |
| 22 | Default security simulator supports every requested security transition without hardware |
| 23 | All nine named root scenarios exist and pass real CLI executions |
| 24 | 76 security pytest cases plus native tests cover all 16 named categories |
| 25 | Session 4999/5000/5001 and challenge 1999/2000/2001 boundaries, equality expires |
| 26 | Requested structured events present; trace tests exclude key/session-token material |
| 27 | Common trace fields; nine decision parity checks and protocol/CRL vectors |
| 28 | All eight requested v0.2 documents present in both projects |
| 29 | AUTH_PROTOCOL actors, messages, complete field encoding and replay explanation |
| 30 | KEY_MANAGEMENT credential-specific keys, key IDs/rotation, fixture/production separation and global-key risk |
| 31 | Threat table T1–T15, assessment labels, explicit glove+credential theft limit |
| 32 | All six named physical/user/RF/radio limitations documented |
| 33 | Six measurement definitions and TBD values in SECURITY_TEST_PLAN |
| 34 | Legacy code/test/scenario preservation audit; PN532/PTT/GPIO/LED build and regression checks; secure default |
| 35 | GPIO/LED only; no actual radio/RF/headset/frequency implementation |
| 36 | Typed values, explicit state machine, separation, standard crypto, deterministic virtual-time tests, fail-closed errors |
| 37 | Actual transport → auth → durable freshness/revocation/binding → session → policy → GPIO structure |
| 38 | Real key possession required by executable secure reference/mock; plain UID cannot authorize |
| 39 | This 14-part report, real build/test/scenario logs, change manifest, limitations and TODO |
| 40 | Excluded integrations absent; actual code/tests/docs implemented; no Git commands |
