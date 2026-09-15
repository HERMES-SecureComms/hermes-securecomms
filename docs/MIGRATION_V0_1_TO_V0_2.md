# v0.1 → v0.2 migration

## Preserve and opt in explicitly to the old behavior

The original README is archived as `V0_1_README.md`. Original architecture,
wiring and manual test documents remain as v0.1 references. Pin mappings,
PN532 initialization, button debounce, TX gate and LEDs remain. UID reads moved
behind `PN532Transport`; `NfcAuth` retains matching and the legacy 1500ms grace.
`PttController.update(bool, ...)` retains the v0.1 path; `updateSecurity` evaluates
a typed secure policy before applying the same output logic.

| Old use | Explicit legacy use | New default |
|---|---|---|
| `pio run` UID auth | `pio run -e uid-poc` | unprovisioned Secure, TX denied |
| `hermes_simulator.cli` UID CLI | `hermes_simulator.legacy_cli` | HMAC secure simulator |
| `Simulator` old facade | `legacy_simulator.Simulator` | secure engine |
| root five scenarios | `scenarios/v0_1/` | nine root security scenarios |
| old example config | `config.uid-poc.example.json` | `config.example.json` secure policy/metadata |

Original test expectations were not weakened or deleted. Only legacy import,
CLI module and fixture/scenario locations were changed to make their intended
mode explicit. The original Python modules `models`, `policy`, `state_machine`,
`clock` and `events` remain available to the legacy implementation.

The new firmware core lives under `firmware/lib/hermes_security/src`, with native
and GPIO tests under `firmware/test`. The two firmware trees are kept identical
as in the original workspace; `tools/validate_security.py` runs the simulator's
copy. They are independent directories, not symlinks or Git submodules.

## Behavioral changes

- UID presence is never secure authentication.
- A fresh secure response creates a fixed-lifetime session; repeated presence
  and held PTT do not extend it. Removal invalidates immediately when observed.
- Expiry is inclusive (`now >= expires_at`). Reauthentication is explicit.
- Per-credential keys, counter storage, CRL trust anchor, metadata/policy versions
  and binding policy must be provisioned. Do not reuse a UID as a key or ID trust proof.
- Invalid authentication, signed-policy input errors, storage failures and
  rollback fail closed. Recovery cannot revive a previous session.
- Legacy serial log strings remain; secure decisions additionally use JSON trace.
- Python now depends on standard `cryptography`; firmware HMAC uses mbedTLS and
  Ed25519 uses Arduino Crypto. No hand-written crypto primitive was added.

## Hardware integration boundary

Secure credential hardware and its APDU/mutual-auth protocol were not supplied.
The default `PN532Transport.respond` and `SecureElementKeyStore` explicitly fail.
Before physical secure authentication, select a card, implement the adapter,
provision a real key store/public CRL anchor and initial NVS state, then validate
latency, removal detection, entropy prerequisites and power-cut recovery.
`secure-mock` is the executable firmware Security PoC; it uses published fixture
keys and resets memory counters on reboot. Never use that environment in production.

Actual radio/RF/headset/audio/frequency configuration, Meshtastic, OpenMANET,
ATAK, dashboard/cloud, Bluetooth, ECG/PPG/UWB, custom PCB and production secure
element deployment remain outside this version. No Git command is needed by any
implementation, build or test step documented here.
