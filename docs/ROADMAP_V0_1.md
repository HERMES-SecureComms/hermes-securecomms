# Roadmap

Each phase is gated by evidence from the preceding phase. Features listed after
v0.1 are direction, not claims about the current repository.

## v0.1 — Functional gate PoC (current)

- UID-based NFC identifier comparison
- PTT gate simulation on an ESP32-S3 GPIO
- Green allow and red deny indications
- Transition-based serial logs and manual test plan

Exit evidence: all cases in `TEST_PLAN.md` pass on documented hardware. UID
matching remains intentionally non-secure.

## v0.2 — Credential security

- Secure NFC credential evaluation
- Cryptographic challenge-response
- Explicit session establishment, expiry, and re-authentication behavior
- Secure-element selection and key lifecycle threat analysis

## v0.3 — Radio/headset electrical interface

- Characterize target PTT voltage, current, polarity, grounding, and keying mode
- Design and validate an isolated/default-off interface using an appropriate
  optocoupler, transistor, MOSFET, or analog switch
- Define headset/radio connector behavior and fault injection tests

No direct GPIO-to-radio connection is permitted during v0.1.

## v0.4 — Fleet and control-plane research

- Device binding and credential-to-device policy
- Remote revocation behavior, including disconnected operation
- Meshtastic control-plane feasibility study

OpenMANET, ATAK, and Bluetooth may be assessed as optional integrations. They
are not dependencies or implemented features of the current PoC.

## v1.0 — Hardened field prototype

- Custom PCB with deterministic hardware default-off behavior
- Hardened enclosure and tamper considerations
- Secure element and protected boot/update chain
- Environmental, usability, security, and field testing

