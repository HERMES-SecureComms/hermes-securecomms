# Security Policy and PoC Limitations

## Status

HERMES PoC v0.1 is a functional breadboard experiment. It does **not** implement
secure operator authentication and is not suitable for operational, safety-
critical, tactical, or production use.

The current code compares an NFC tag UID with a value compiled into firmware.
`AUTH SUCCESS` means only “the observed bytes match the configured bytes.” It
must not be interpreted as a cryptographic assertion of identity.

## Known and intended limitations

- **UID cloning:** ISO14443A UIDs can be observed and, for compatible tags,
  copied or emulated. A whitelist does not establish authenticity.
- **No replay resistance:** There is no nonce, counter, freshness proof, mutual
  authentication, or cryptographic challenge-response.
- **No secure element:** The design has no protected key storage or isolated
  cryptographic operations. The example UID is not a key.
- **No physical-tampering defense:** Exposed SPI, GPIO, flash, and breadboard
  wiring can be probed, replaced, or manipulated.
- **No credential-theft defense:** Possession of the configured/cloned tag is
  sufficient for the PoC check; there is no PIN, biometric, liveness, or second
  factor.
- **No firmware trust chain:** Secure boot, flash encryption, signed update
  policy, rollback prevention, and debug lockdown are not configured here.
- **No radio integration:** GPIO5 is a simulation output only. Radio electrical
  safety, isolation, fault containment, and RF behavior have not been assessed.
- **Grace-window exposure:** A last valid UID read remains authorized for the
  configured 1,500 ms to tolerate NFC read instability. This is not a designed
  security session timeout.
- **No runtime PN532 integrity monitoring:** Initialization failure latches TX
  off, but ordinary “no tag” polling responses cannot by themselves distinguish
  removal, interference, wiring faults, or active manipulation.

These are intentional constraints of PoC v0.1, not defects hidden behind a
security claim. The next credential phase is intended to replace UID comparison
with Secure NFC, cryptographic challenge-response, explicit session management,
and secure-element-backed keys after a threat model and protocol review.

## Fail-closed claims and boundary

The firmware commands `TX_GATE` LOW before peripheral setup, whenever PTT is
released, whenever authorization is false or expired, and permanently after a
PN532 initialization failure. Unknown logical states therefore do not command
TX HIGH.

This software invariant does not guarantee a physical voltage during chip reset,
loss of power, pin damage, malicious firmware, or external shorting. A real
system needs a separately validated hardware default-off circuit and independent
safety analysis.

## Safe testing

> **ESP32 GPIO를 실제 무전기의 PTT 라인에 직접 연결하지 말 것.**

Do not connect GPIO5—or any ESP32 GPIO—directly to a real radio PTT line. Limit
testing to LEDs and high-impedance instruments. Radio integration requires prior
electrical characterization and an appropriate isolated interface.

Do not use this PoC to protect sensitive or emergency communications. Test only
with hardware and spectrum usage for which you have authorization; this version
does not generate RF.

## Reporting security issues

When opening a private security report for a future hosted repository, include
the affected commit, hardware configuration, reproduction steps, observed
output, expected fail-closed behavior, and whether GPIO5 was electrically
monitored. Do not include real keys, sensitive radio configuration, or personal
credential data in a public issue.

