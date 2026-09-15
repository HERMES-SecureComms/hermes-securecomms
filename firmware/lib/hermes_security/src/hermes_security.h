#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace hermes { namespace security {
using Bytes = std::vector<uint8_t>;
enum class Status : uint8_t { Active, Revoked, Disabled };
enum class BindingMode : uint8_t { Disabled, AllowList };
enum class Mode : uint8_t { NORMAL, DEGRADED, REVOKED };
enum class SessionState : uint8_t { NONE, ACTIVE, EXPIRED, REVOKED };
enum class Decision : uint8_t {
  ALLOW_TX, ALLOW_EMERGENCY_ONLY, DENY_NO_CREDENTIAL, DENY_AUTH_FAILED,
  DENY_REPLAY, DENY_COUNTER_ROLLBACK, DENY_CREDENTIAL_REVOKED, DENY_DEVICE_REVOKED,
  DENY_BINDING, DENY_SESSION_EXPIRED, DENY_SYSTEM_FAILURE, DENY_POLICY_ROLLBACK,
  DENY_REAUTHENTICATION_REQUIRED, DENY_PTT_RELEASED, DENY_DEGRADED
};
const char* decisionName(Decision value);
struct Credential {
  std::string credentialId, operatorId, keyId;
  Status status = Status::Active;
  uint64_t counter = 0;
  std::vector<std::string> allowedDeviceIds;
  uint64_t metadataVersion = 1;
};
struct Policy {
  uint64_t version = 1, sessionTimeoutMs = 5000, challengeTimeoutMs = 2000;
  BindingMode bindingMode = BindingMode::AllowList;
  Mode mode = Mode::NORMAL;
  bool emergencyEnabled = false;
};
struct Challenge {
  uint16_t protocolVersion = 2;
  std::string deviceId;
  Bytes nonce;
  uint64_t policyVersion = 1;
  Bytes sessionContext;
};
struct Response {
  std::string credentialId, keyId;
  uint64_t counter = 0;
  Bytes mac;
};
struct Session {
  Bytes sessionId;
  std::string credentialId, operatorId, deviceId;
  uint64_t createdAt = 0, expiresAt = 0, policyVersion = 0;
};
struct RevocationEntry { std::string credentialId; uint64_t revokedAt = 0; std::string reason; };
struct RevocationList { uint64_t version = 0, issuedAt = 0; std::vector<RevocationEntry> entries; Bytes signature; };
struct PersistentState {
  std::string deviceId;
  Policy policy;
  std::vector<Credential> credentials;
  Status deviceStatus = Status::Active;
  RevocationList crl;
  std::vector<std::string> revoked;
};
struct SecurityContext {
  bool systemHealthy = false;
  Status deviceStatus = Status::Disabled, credentialStatus = Status::Disabled;
  bool credentialPresent = false, credentialAuthenticated = false;
  bool bindingValid = false, counterFresh = false, sessionBound = false;
  SessionState sessionState = SessionState::NONE;
  uint64_t sessionDeadlineMs = 0, nowMs = 0;
  bool pttPressed = false;
  Mode mode = Mode::NORMAL;
  bool emergencyEnabled = false;
  Decision denial = Decision::DENY_NO_CREDENTIAL;
};
Decision evaluate(const SecurityContext& ctx);
bool validIdentifier(const std::string& value);
bool validPolicy(const Policy& policy);
bool validCredential(const Credential& credential);
Bytes transcript(const Challenge&, const Response&);
Bytes revocationPayload(const RevocationList&);
Bytes encodeState(const PersistentState&);
bool decodeState(const Bytes&, PersistentState&);
namespace crypto {
bool random(Bytes& out, size_t length);
bool hmacSha256(const Bytes& key, const Bytes& data, Bytes& mac);
bool ed25519Verify(const Bytes& publicKey, const Bytes& data, const Bytes& signature);
bool equal(const Bytes& a, const Bytes& b);
}
enum class Verification { VALID, INVALID, ERROR };
class KeyStore {
 public:
  virtual ~KeyStore() = default;
  virtual Verification verifyCredential(const Credential&, const Bytes&, const Bytes&) = 0;
};
class SoftwareKeyStore : public KeyStore {
 public:
  // PoC/test only; key material is never compiled into default firmware.
  bool add(const std::string& credentialId, const std::string& keyId, const Bytes& key);
  Verification verifyCredential(const Credential&, const Bytes&, const Bytes&) override;
 private:
  std::map<std::pair<std::string, std::string>, Bytes> keys_;
};
class SecureElementKeyStore : public KeyStore {
 public:
  Verification verifyCredential(const Credential&, const Bytes&, const Bytes&) override { return Verification::ERROR; }
};
class StateStore {
 public:
  virtual ~StateStore() = default;
  virtual bool load(PersistentState&) = 0;
  virtual bool save(const PersistentState&) = 0;
};
class MemoryStateStore : public StateStore {
 public:
  PersistentState state;
  bool failed = false;
  bool load(PersistentState& out) override { if (failed) return false; out = state; return true; }
  bool save(const PersistentState& in) override { if (failed) return false; state = in; return true; }
};
class CredentialTransport {
 public:
  virtual ~CredentialTransport() = default;
  virtual bool respond(const Challenge&, Response&) = 0;
};
class MockCredentialTransport : public CredentialTransport {
 public:
  MockCredentialTransport(Credential credential, Bytes key, StateStore& store)
      : credential_(credential), key_(key), store_(store) {}
  bool respond(const Challenge&, Response&) override;
 private:
  Credential credential_;
  Bytes key_;
  StateStore& store_;
};
struct Event { uint64_t timestampMs; std::string event, deviceId, credentialId, reason; };
using EventSink = std::function<void(const Event&)>;
class Engine {
 public:
  Engine(std::string deviceId, KeyStore&, StateStore&, Bytes revocationPublicKey, EventSink sink = {});
  bool begin(uint64_t now);
  bool beginChallenge(Challenge&, uint64_t now);
  bool validateResponse(const Response&, uint64_t now);
  bool authenticate(CredentialTransport&, uint64_t now);
  Decision tick(uint64_t now, bool pttPressed);
  SecurityContext context(uint64_t now, bool pttPressed) const;
  bool sessionMatches(const Bytes&, const std::string& deviceId, const std::string& credentialId, uint64_t now);
  void detach(uint64_t now);
  void fault(uint64_t now, const char* reason = "SYSTEM_FAILURE");
  bool recover(uint64_t now);
  bool updatePolicy(const Policy&, uint64_t now);
  bool updateCredential(const Credential&, uint64_t now);
  bool applyRevocationList(const RevocationList&, uint64_t now);
  bool setDeviceStatus(Status, uint64_t now);
  const Session& session() const { return session_; }
  SessionState sessionState() const { return sessionState_; }
  const PersistentState& state() const { return state_; }
  const std::string& reason() const { return reason_; }
  bool healthy() const { return healthy_; }
 private:
  Credential* credential(const std::string& id);
  const Credential* credential(const std::string& id) const;
  bool binding(const Credential&) const;
  bool revoked(const Credential&) const;
  bool persist(const PersistentState&, uint64_t now);
  void invalidate(Decision, const char*, SessionState = SessionState::NONE);
  bool deny(Decision, const char* reason, const char* event, uint64_t now);
  bool rollback(uint64_t now);
  void emit(const char*, const char*, uint64_t now);
  std::string deviceId_, attached_, reason_ = "NO_CREDENTIAL";
  KeyStore& keys_;
  StateStore& store_;
  Bytes publicKey_;
  EventSink sink_;
  PersistentState state_;
  Session session_;
  SessionState sessionState_ = SessionState::NONE;
  Challenge pending_;
  uint64_t challengeDeadline_ = 0, lastNow_ = 0;
  bool hasPending_ = false, healthy_ = false, controlFault_ = false, pressed_ = false;
  Decision denial_ = Decision::DENY_NO_CREDENTIAL, lastDecision_ = Decision::DENY_NO_CREDENTIAL;
};
} }
