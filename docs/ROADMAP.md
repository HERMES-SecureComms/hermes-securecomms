# Roadmap

- **v0.1 (preserved):** UID functional demo and GPIO/PTT/LED regression mode.
- **v0.2 (this Security PoC):** per-credential HMAC authentication, replay/counter
  controls, Ed25519 CRLs, version checks, device binding, fixed sessions, pure
  policy, simulator and portable firmware tests. Hardware adapters are explicit
  unprovisioned boundaries; `secure-mock` executes the firmware security flow.
- **v0.3 security hardening:** selected secure NFC adapter, real secure-element
  KeyStore, provisioning, Secure Boot V2, Flash/NVS Encryption, secure updates,
  persistent counter/CRL power-loss and wear validation, bounded NFC scheduling.
- **v1.0 assurance work:** physical tamper/default-off design, hardware
  anti-rollback, protected key lifecycle, authority rotation, on-device security
  review and measured performance. None is a claim of this PoC.

[Threat model and hardening details](THREAT_MODEL_V0_2.md).
The earlier exploratory roadmap is retained in [ROADMAP_V0_1.md](ROADMAP_V0_1.md).
Radio/RF, network integrations, dashboard/cloud and custom hardware are outside
this v0.2 implementation.
