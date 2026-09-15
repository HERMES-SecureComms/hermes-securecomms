# HERMES v0.2 threat model

## Assumptions

The PoC trusts its running verifier code, local provisioning/admin APIs, clock,
cryptographic libraries and its remaining persistent state. The attacker may
present arbitrary credential responses or old/modified CRLs. They may steal
hardware; software-only controls do not make stolen hardware tamper-resistant.
There is no actual RF/radio/headset integration. “Mitigated” below is limited to
these assumptions, not a production assurance level.

| ID | Threat | Assessment | Evidence and remaining boundary |
|---|---|---|---|
| T1 | Radio theft | Not Mitigated | No actual radio control or RF-network enforcement exists |
| T2 | PTT theft | Partially Mitigated | Without credential key, intact firmware denies; extraction/modification remains |
| T3 | Unknown credential | Mitigated | Metadata lookup and key ID check deny unknown responses |
| T4 | UID clone | Mitigated | UID cannot create a secure session; possession of the HMAC key is required |
| T5 | Replay | Mitigated | Fresh nonce/context, single-use challenge, counter and timeout rejection tests |
| T6 | Credential revocation | Partially Mitigated | Applied signed CRL denies immediately; offline devices may have stale lists |
| T7 | Device revocation | Partially Mitigated | Trusted local persistent state denies; no remote distribution or tamper-proof state |
| T8 | Session replay | Mitigated | Session never imported; exact local handle + device + credential binding |
| T9 | Device binding bypass | Partially Mitigated | Authenticated device ID and configured allowlist; trusted device identity may be modified physically |
| T10 | Counter rollback | Partially Mitigated | Strict persistent high-water mark; whole-disk/flash rollback not prevented |
| T11 | Revocation rollback | Partially Mitigated | Signed stale update rejected; physical state rollback and offline freshness remain |
| T12 | Firmware modification | Not Mitigated | Attacker can modify firmware to bypass the Policy Engine; secure boot is not enabled |
| T13 | Secure storage compromise | Not Mitigated | Software keys and ordinary state storage are extractable/replaceable |
| T14 | Glove + Credential theft | Not Mitigated | A thief holding the genuine key-bearing credential can authenticate until revoked |
| T15 | Physical tampering | Not Mitigated | Exposed wiring, forced GPIO, fault injection and invasive extraction are not blocked |

A stolen glove plus credential is **not fully solved in v0.2**. Credential and
user capture together, physical coercion, advanced hardware side channels,
secure-element physical extraction, full RF network compromise and radio firmware
compromise remain outside the implemented guarantees. A live relay is not an
old-response replay and is not prevented by this protocol. Availability under
jamming/flooding is not guaranteed. Fail-closed behavior can itself cause denial
of service; emergency-only is only an explicit decision state, not a radio path.

## ESP32-S3 firmware integrity research and next hardening

Secure Boot V2 verifies signed executable images using an eFuse-rooted trust
configuration. It addresses replacement of bootloader/application code when
correctly provisioned. Flash Encryption protects off-chip flash confidentiality;
it is not a substitute for executable signature verification or authorization.
This build enables neither feature and performs no eFuse operations.
[Espressif Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/security/secure-boot-v2.html),
[ESP-IDF 4.4 Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/security/flash-encryption.html).

v0.3/v1.0 hardening work:

1. Select secure credential hardware and implement its authenticated transport;
   preserve the `CredentialTransport`/`KeyStore` boundary if hardware supplies
   AES mutual authentication instead of this reference HMAC protocol.
2. Hardware-rooted provisioning, separate per-credential keys, controlled rotation,
   protected device identity and a real secure-element verifier adapter.
3. Secure Boot V2 plus Flash Encryption, protected update/signing-key lifecycle,
   firmware rollback controls, debug/download interface policy and recovery design.
4. Encrypted authenticated state, hardware monotonic anti-rollback, power-cut and
   flash-wear tests, CRL distribution/freshness policy and authority-key rotation.
5. Tamper-resistant default-off output, fault injection assessment and bounded
   transport scheduling; measure timing on a physical board.

These are future requirements, not features silently enabled by `pio run`.
