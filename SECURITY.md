# Security status — v0.2

This is a Security PoC, unsuitable for production, tactical or safety-critical
use. It models cryptographic credential possession and simulated PTT permission;
it does not establish a human identity or protect an actual RF network.

HMAC challenge-response, replay checks, signed CRLs, version checks, binding,
fixed-lifetime sessions and fail-closed policies are implemented in the reference
and portable firmware core. Default hardware firmware is unprovisioned and denies.
The secure NFC and secure-element adapters await selected hardware. Mock fixtures
are public test keys. **SoftwareKeyStore must not be used in production.**

Read [the v0.2 threat model](docs/THREAT_MODEL_V0_2.md) and
[key/storage limitations](docs/KEY_MANAGEMENT.md). Firmware replacement can bypass
the policy engine; no secure boot, flash/NVS encryption, tamper defense or hardware
anti-rollback is enabled. Genuine credential theft, coercion, live relay, advanced
side channels, physical extraction and RF/radio firmware compromise remain.

Only GPIO/LED simulation is supported. External wiring, reset, power failure,
pin damage or malicious firmware can defeat a software-only default-off rule.
Never connect GPIO directly to real radio PTT equipment.

When reporting a security issue, include the configuration, protocol/state
transition, expected denial and observed result. Do not include production keys,
private credential data or sensitive radio settings. Legacy limitations are
archived in [SECURITY_V0_1.md](docs/SECURITY_V0_1.md).
