# Security test and measurement plan

## Reproducible commands

From `hermes-simulator` with Python 3.12+ and a C++17 compiler/OpenSSL development
headers installed:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/python -m pytest -q
.venv/bin/python tools/validate_security.py
.venv/bin/python -m hermes_simulator.cli run scenarios/replay_attack.json
.venv/bin/python -m hermes_simulator.cli run scenarios/credential_revoked.json
.venv/bin/python -m hermes_simulator.cli run scenarios/session_expired_during_tx.json
.venv/bin/python -m hermes_simulator.cli run scenarios/revocation_list_rollback.json
```

Firmware, from either project's `firmware` directory:

```sh
pio run -e esp32-s3-devkitc-1 -e uid-poc -e secure-mock
bash test/native/run.sh
```

`pio` requires PlatformIO Core. Builds do not upload or operate physical hardware.
The native runner uses `-Wall -Wextra -Werror`, AddressSanitizer and UndefinedBehaviorSanitizer.
Scenario runners return nonzero on expectation failures; expectations are
prevalidated before the provided simulator is mutated. Fixed protocol values are
only test fixtures. Virtual time is deterministic; runtime nonces remain CSPRNG.

## Coverage

| Category | Evidence |
|---|---|
| Authentication / IDs / MAC context | Python context-field tests; C++ transcript vector and HMAC verification |
| Replay / nonce uniqueness | captured accepted/unaccepted response on new challenge; consumed attempts; unique nonce/context/session sets |
| Counter freshness / rollback | accepted 10, deny 9/equal 10, accept 11; overflow and durable commit failure |
| Session binding | wrong device, wrong credential, wrong ID, independent verifier and corrupted session context |
| Session expiration | 4999 / 5000 / 5001ms while held PTT; status read and exact expiry |
| Credential / device revocation | valid crypto credential denied by status; signed CRL during TX; persisted device status |
| Device binding | allowlist match/mismatch/empty, explicit disabled binding |
| Signed CRL | valid, wrong signature/key, modified payload, invalid shape, saved payload verification |
| Anti-rollback | lower/equal policy, CRL and metadata versions; floors survive store reopen |
| System failure / recovery | reader, keystore, storage, RNG, clock errors; session/challenge cleared; explicit reauth |
| Fail-closed / modes | all normal policy boolean gates, corrupted typed context, NORMAL/DEGRADED/REVOKED |
| Power-loss preparation | atomic file store, single-writer lock, missing/corrupt file, truncated C++ state snapshots |
| v0.1 regression | original 97 assertions/tests routed to explicit legacy implementation; UID firmware build |
| Hardware control logic | actual `PttController` linked to fake GPIO: boot low, debounce, release, auth loss, permanent fault, secure expiry |
| Cross-platform | nine golden decisions + identical protocol transcript/MAC and signed CRL fixtures |

See `VALIDATION_REPORT.md` for the actual latest execution results. Passing host
tests proves the modeled transitions, not physical NFC behavior or resistance to
hardware attacks. NVS power interruption, real NFC interoperability and the
ESP32 crypto backends need on-device validation once hardware is available.

## Future hardware metrics — all values TBD

| Metric | Start / end | Value |
|---|---|---|
| Authentication Latency | challenge generation starts / response verified and counter committed | TBD |
| Session Creation Latency | successful MAC verification / session published | TBD |
| PTT-to-TX Decision Latency | debounced press / policy decision and GPIO transition | TBD |
| Replay Detection Latency | stale response received / denial emitted | TBD |
| Revocation Enforcement Time | signed update received / active GPIO driven LOW | TBD |
| Credential Removal → TX Disable Time | independently observed physical removal / GPIO LOW | TBD |

Use logic-analyzer GPIO capture plus monotonic trace timestamps; record firmware,
policy/config, NFC transport, power conditions, trial count and distributions.
Do not substitute desktop test duration for hardware authentication latency.
