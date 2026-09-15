# Key management

## Hierarchy and separation

```text
Device Root Key (future hardware-protected wrapping/provisioning trust)
    -> independently provisioned Credential-specific Key
    -> HMAC authentication
    -> ephemeral, device-bound Session Context (no session encryption key)
```

Each credential/key ID pair uses its own 256-bit key. A global shared key would
let extraction from one glove impersonate every glove; both software stores
reject duplicate key material when registering keys. Key IDs name key versions,
for example `KEY-GLOVE-001-v1`; they do not contain or derive a secret by themselves.

The Python provisioning helper `derive_credential_key` uses HKDF-SHA256 with a
32-byte root, fixed public salt `HERMES-KDF-v2` and length-prefixed credential/key
IDs as `info`. Changing either ID changes the derived key. This helper is an
optional single-device provisioning reference, not default runtime key storage.
HKDF is the standard extract/expand construction from
[RFC 5869](https://www.rfc-editor.org/info/rfc5869/).

For multiple allowed PTTs, the same credential-specific key must be provisioned
to each approved verifier under its own device-root protection, or a future
credential driver must select separately provisioned per-device keys. Deriving
from unrelated device roots does not magically produce the same credential key.
Root wrapping, secure injection and device-root lifecycle are design boundaries,
not production facilities implemented by this PoC.

## Test and production material

- Simulator keys are independently generated in memory with `secrets` on each
  new simulation. They have no production validity.
- Fixed MAC test keys and the public RFC 8032 Ed25519 signing test seed exist only
  in `tests/fixtures`. Firmware mock keys are under `firmware/test/fixtures` and
  are compiled only with `secure-mock`.
- A public CRL verification key is not a secret. The demo public key trusts only
  the published test fixture signatures; never deploy it as a real authority.
- **SoftwareKeyStore is PoC/test-only; production use is prohibited.** Python
  memory and C++ software memory do not prevent extraction or guarantee erasure.
- Production secrets are absent. `.gitignore` excludes local key/state files,
  key directories, PEM/private files and `local_security_config.h`. Git ignore
  is an accident-prevention rule, not encryption or access control.
- `SecureElementKeyStore` exposes verification without exposing raw keys. The
  current adapter returns an error until a real hardware backend is implemented.

## Rotation and counters

A trusted administrator provisions a new unique key/key ID, increases credential
metadata version and updates verifier metadata. Active sessions are invalidated;
old key IDs fail. Keep the credential's last accepted counter across key rotation
and metadata updates. An old metadata version or equal-version replacement is
rejected. Revocation is cumulative: metadata updates cannot silently un-revoke a
credential. Authority-key rotation and a signed provisioning channel are future
work; never reset trust anchors or counters merely to resolve an error.

The credential persists its next counter **before returning** its response.
The verifier persists last accepted counter **before creating** its session.
A power failure in between may consume a counter with no session; gaps are safe.
No session or secret is included in the public persistent state snapshot.

`FileStore(path, initialize=True)` explicitly provisions a new empty store;
normal `FileStore(path)` refuses missing/corrupt state. It uses a single-writer
lock, private temporary file, fsync, atomic replace and directory fsync. The
state's device ID prevents opening another device's snapshot as this PTT.
The in-memory simulator and firmware mock are explicitly non-durable.

`NvsStateStore` reads/writes one validated snapshot and calls `nvs_commit`.
Missing state, malformed data, capacity exhaustion or an I/O error denies TX;
there is no automatic erase/reset. Actual power-cut behavior and wear must be
measured on hardware. ESP-IDF describes NVS recovery and its security limits in
[NVS documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/storage/nvs_flash.html).

Restoring an entire older valid file/flash image, or erasing state and deliberately
reprovisioning it, is not prevented by software version floors. Hardware monotonic
storage, authenticated encrypted storage, secure boot and provisioning controls
are required for that threat. This PoC detects stale updates against the state
it still trusts; it does not claim physical anti-rollback protection.
