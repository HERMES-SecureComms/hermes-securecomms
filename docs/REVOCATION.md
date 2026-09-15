# Signed revocation and anti-rollback

The offline authority supplies a JSON envelope:

```json
{
  "payload": {
    "version": 11,
    "issued_at": 100,
    "entries": [{"credential_id": "GLOVE-001", "revoked_at": 90, "reason": "lost credential"}]
  },
  "signature": "<128 hexadecimal characters: Ed25519 signature>"
}
```

`issued_at` and `revoked_at` are unsigned milliseconds since the authority's
chosen epoch (deployment convention: Unix epoch). Fixture values are artificial.
`revoked_at <= issued_at` is required. No synchronized wall clock or CRL age
expiry guarantee is claimed; freshness is strictly by stored version.

## Exact signed bytes

```text
ASCII("HERMES-CRL-v2") || 0x00 ||
u64be(version) || u64be(issued_at) || u16be(entry_count) ||
for each entry, in supplied order:
    text(credential_id) || u64be(revoked_at) || text(reason)
```

`text` is the same length-prefixed ASCII encoding as the auth protocol. Reason is
1–128 printable ASCII characters, including spaces. IDs use the restricted
identifier syntax. Duplicate IDs, extra JSON fields and more than 256 entries
are invalid. The version is positive. The signature is Ed25519 over the exact
binary payload, **not** over pretty-printed JSON. Python uses `cryptography`;
ESP32 uses the standard Ed25519 implementation in Arduino Crypto; native C++
uses OpenSSL. See [Ed25519 API](https://cryptography.io/en/latest/hazmat/primitives/asymmetric/ed25519/)
and [RFC 8032](https://www.rfc-editor.org/info/rfc8032/).

Validation order is shape/encoding, signature, strictly newer version, durable
snapshot commit, then credential revocation enforcement. A modified payload or
wrong signature never changes the stored version. It faults the verifier and
removes the active session. A valid older **or equal** version returns
`REJECT_ROLLBACK`, retains the accepted list and invalidates authorization.
The test fixture private signing seed is confined to `tests/fixtures`.

Revocations accumulate: a higher list omitting a previously revoked credential
does not restore it. Unknown revoked IDs are retained so future metadata cannot
make them active accidentally. State capacity is 256 cumulative revoked IDs;
exhaustion fails closed. Active and future sessions for revoked IDs are denied.
A device's local REVOKED/DISABLED state also persists and overrides credentials.
Device restore is a trusted local operation and still requires reauthentication.

## Three independent version floors

| State | Scope | Update rule |
|---|---|---|
| Policy version | PTT device | incoming > currently stored |
| Revocation list version | configured authority / PTT | valid signature, then incoming > stored |
| Credential metadata version | each credential | incoming > current credential version |

Versions and the corresponding payloads are saved together, rather than storing
only an in-memory floor. Counter high-water marks are retained during metadata
updates. Boot/recovery verifies the saved CRL signature and snapshot consistency.
Policy/metadata updates are local trusted APIs; remote policy-signature
verification is not implemented. Power-loss and physical snapshot rollback
limitations are detailed in [KEY_MANAGEMENT](KEY_MANAGEMENT.md).

Run `credential_revoked.json`, `revocation_list_rollback.json` and
`tampered_revocation_list.json` with the secure simulator. The signed v9/v10/v11
fixtures are public examples, not production authority documents.
