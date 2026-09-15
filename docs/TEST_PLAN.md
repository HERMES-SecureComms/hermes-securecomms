> v0.1 reference retained for regression. Current security design: [v0.2 architecture](V0_2_ARCHITECTURE.md) and [security tests](SECURITY_TEST_PLAN.md).

# HERMES PoC v0.1 Test Plan

## Purpose and acceptance rule

These manual tests verify that only the configured NFC UID plus a pressed PTT
can assert the simulated `TX_GATE`. The overall acceptance rule is strict: any
unexpected HIGH on GPIO5 is a failure and must be investigated before continuing.

No test connects a radio or RF transmitter.

## Equipment and setup

- Fully wired ESP32-S3/PN532 breadboard from `WIRING.md`
- One tag whose UID is compiled into `credentials.h`
- At least one tag with a different UID
- 115200-baud serial monitor
- Logic analyzer or high-impedance multimeter on GPIO5 (recommended)
- Stopwatch or captured logic trace for grace-period checks

Record the firmware commit, board model, PN532 module model/mode, configured UID,
and actual grace/debounce values with each run. The default grace period is
1,500 ms. Exact PTT-to-TX performance is not yet specified; report it as TBD
unless measured under a documented method.

## Test cases

### TC-001 — Authorized credential + PTT

**Steps:** Present the configured tag, wait for `AUTH SUCCESS`, press and hold
PTT, then release it.

**Expected:** While held, GPIO5 is HIGH, green is ON, red is OFF, and one
`TX ENABLED` transition is logged. On release, GPIO5 becomes LOW, green turns
OFF, and one `TX DISABLED` transition is logged.

### TC-002 — Unauthorized credential + PTT

**Steps:** Present a tag with a different UID and press PTT.

**Expected:** `AUTH FAILED` and `TX DENIED - NO VALID CREDENTIAL` are logged;
GPIO5 stays LOW, green stays OFF, and red is ON while PTT is held.

### TC-003 — No NFC + PTT

**Steps:** Remove all tags, wait at least 1,500 ms after any previous authorized
read, and press PTT.

**Expected:** GPIO5 stays LOW, green stays OFF, red turns ON, and TX denial is
logged once for the press.

### TC-004 — Authorized credential removed

**Steps:** Present the configured tag, observe `AUTH SUCCESS`, then remove it
without pressing PTT.

**Expected:** Authorization remains only through brief missed reads and expires
at approximately the configured 1,500 ms grace deadline. `CREDENTIAL LOST` is
logged once. No exact security-session timing claim is inferred from this test.

### TC-005 — Credential restored

**Steps:** Complete TC-004, then present the configured tag again.

**Expected:** The UID and `AUTH SUCCESS` are logged again and authorization is
restored. A subsequent PTT press enables simulated TX.

### TC-006 — ESP32 reboot

**Steps:** Monitor GPIO5 and reset or power-cycle the ESP32-S3, with PTT released
and then in a separate run held.

**Expected:** Firmware never commands GPIO5 HIGH during boot; boot logs begin
with `TX DISABLED`. Holding PTT cannot enable TX before successful PN532 setup
and a valid UID observation. Record any reset-time electrical transient; future
hardware requires a physical default-off circuit.

### TC-007 — PN532 initialization failure

**Steps:** Power down, disconnect PN532 SS or power, restart, and press PTT.

**Expected:** A PN532 error and `TX PERMANENTLY DISABLED` are logged. GPIO5 stays
LOW, green stays OFF, and red remains ON. Reconnecting the PN532 without reset
does not clear the latched fault.

### TC-008 — PTT held while credential disappears

**Steps:** Enable TX with the configured tag and keep PTT held while removing
the tag.

**Expected:** GPIO5 and green go LOW after authorization reaches the 1,500 ms
grace deadline; `CREDENTIAL LOST`, `TX DISABLED`, and one denial transition are
logged. Red turns ON. TX must not remain enabled beyond that decision loop.

### TC-009 — Repeated rapid PTT presses

**Steps:** With no valid credential, bounce or press/release PTT rapidly around
the 30 ms debounce interval. Repeat with a valid credential.

**Expected:** Without a valid UID, GPIO5 never goes HIGH. With a valid UID, each
stable press/release can enable/disable TX, without stuck output or log flooding.

### TC-010 — Different NFC UID

**Steps:** First authorize with the configured tag. Replace it directly with a
different tag, including once while PTT is held.

**Expected:** The different UID is printed and authorization is revoked
immediately rather than after the grace interval. `AUTH FAILED` is logged and
GPIO5 remains or becomes LOW.

## Results record

| Test | Pass/Fail | Evidence (serial capture/trace) | Notes |
|---|---|---|---|
| TC-001 |  |  |  |
| TC-002 |  |  |  |
| TC-003 |  |  |  |
| TC-004 |  |  |  |
| TC-005 |  |  |  |
| TC-006 |  |  |  |
| TC-007 |  |  |  |
| TC-008 |  |  |  |
| TC-009 |  |  |  |
| TC-010 |  |  |  |

Hardware acceptance remains pending until every case is executed on the stated
equipment. A successful firmware build alone does not prove electrical behavior.

