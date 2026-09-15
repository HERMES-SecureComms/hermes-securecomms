#include "hermes_security.h"
#include <algorithm>
#include <limits>

namespace hermes { namespace security {
const char* decisionName(Decision value) {
  static const char* names[]={"ALLOW_TX","ALLOW_EMERGENCY_ONLY","DENY_NO_CREDENTIAL","DENY_AUTH_FAILED",
    "DENY_REPLAY","DENY_COUNTER_ROLLBACK","DENY_CREDENTIAL_REVOKED","DENY_DEVICE_REVOKED","DENY_BINDING",
    "DENY_SESSION_EXPIRED","DENY_SYSTEM_FAILURE","DENY_POLICY_ROLLBACK","DENY_REAUTHENTICATION_REQUIRED",
    "DENY_PTT_RELEASED","DENY_DEGRADED"};
  auto n=static_cast<size_t>(value); return n<sizeof(names)/sizeof(names[0]) ? names[n] : "DENY_SYSTEM_FAILURE";
}
Decision evaluate(const SecurityContext& c) {
  if(!c.systemHealthy) return Decision::DENY_SYSTEM_FAILURE;
  if(static_cast<uint8_t>(c.deviceStatus)>2 || static_cast<uint8_t>(c.mode)>2 ||
     static_cast<uint8_t>(c.sessionState)>3 || static_cast<uint8_t>(c.denial)>14) return Decision::DENY_SYSTEM_FAILURE;
  if(c.deviceStatus!=Status::Active || c.mode==Mode::REVOKED) return Decision::DENY_DEVICE_REVOKED;
  if(c.credentialStatus==Status::Revoked) return Decision::DENY_CREDENTIAL_REVOKED;
  if(c.denial==Decision::DENY_POLICY_ROLLBACK) return c.denial;
  if(c.mode==Mode::DEGRADED) return c.emergencyEnabled && c.pttPressed ? Decision::ALLOW_EMERGENCY_ONLY : Decision::DENY_DEGRADED;
  if(!c.credentialAuthenticated) return static_cast<uint8_t>(c.denial)>=2 ? c.denial : Decision::DENY_AUTH_FAILED;
  if(!c.credentialPresent) return Decision::DENY_NO_CREDENTIAL;
  if(c.credentialStatus!=Status::Active) return Decision::DENY_AUTH_FAILED;
  if(!c.bindingValid || !c.sessionBound) return Decision::DENY_BINDING;
  if(!c.counterFresh) return Decision::DENY_REPLAY;
  if(!c.sessionDeadlineMs) return Decision::DENY_SYSTEM_FAILURE;
  if(c.sessionState==SessionState::EXPIRED || c.nowMs>=c.sessionDeadlineMs) return Decision::DENY_SESSION_EXPIRED;
  if(c.sessionState!=SessionState::ACTIVE) return Decision::DENY_REAUTHENTICATION_REQUIRED;
  return c.pttPressed ? Decision::ALLOW_TX : Decision::DENY_PTT_RELEASED;
}
bool SoftwareKeyStore::add(const std::string& id,const std::string& keyId,const Bytes& key) {
  if(!validIdentifier(id) || !validIdentifier(keyId) || key.size()!=32) return false;
  for(const auto& entry:keys_) if(crypto::equal(entry.second,key)) return false;
  return keys_.emplace(std::make_pair(id,keyId),key).second;
}
Verification SoftwareKeyStore::verifyCredential(const Credential& c,const Bytes& data,const Bytes& mac) {
  auto it=keys_.find({c.credentialId,c.keyId}); Bytes expected;
  if(it==keys_.end() || data.empty() || !crypto::hmacSha256(it->second,data,expected)) return Verification::ERROR;
  return crypto::equal(expected,mac) ? Verification::VALID : Verification::INVALID;
}
bool MockCredentialTransport::respond(const Challenge& challenge,Response& response) {
  PersistentState state;
  if(!store_.load(state)) return false;
  auto it=std::find_if(state.credentials.begin(),state.credentials.end(),[this](const Credential& c){return c.credentialId==credential_.credentialId;});
  if(it==state.credentials.end()) return false;
  if(it->counter==std::numeric_limits<uint64_t>::max()) return false;
  ++it->counter; response={credential_.credentialId,credential_.keyId,it->counter,{}};
  if(!store_.save(state)) return false;
  auto data=transcript(challenge,response);
  return !data.empty() && crypto::hmacSha256(key_,data,response.mac);
}
Engine::Engine(std::string id,KeyStore& keys,StateStore& store,Bytes publicKey,EventSink sink)
    : deviceId_(id),keys_(keys),store_(store),publicKey_(publicKey),sink_(sink) {}
bool Engine::begin(uint64_t now) { return recover(now); }
bool Engine::recover(uint64_t now) {
  PersistentState loaded,validated;
  if(!store_.load(loaded) || !decodeState(encodeState(loaded),validated) || loaded.deviceId!=deviceId_ || publicKey_.size()!=32 ||
      (loaded.crl.version && !crypto::ed25519Verify(publicKey_,revocationPayload(loaded.crl),loaded.crl.signature))) {
    fault(now,"SECURE_STORAGE_ERROR"); return false;
  }
  state_=validated; healthy_=true; controlFault_=false; lastNow_=now;
  invalidate(Decision::DENY_REAUTHENTICATION_REQUIRED,"REAUTHENTICATION_REQUIRED");
  emit("SYSTEM_RECOVERED","REAUTHENTICATION_REQUIRED",now); tick(now,pressed_); return true;
}
Credential* Engine::credential(const std::string& id) {
  for(auto& c:state_.credentials) if(c.credentialId==id) return &c;
  return nullptr;
}
const Credential* Engine::credential(const std::string& id) const {
  for(const auto& c:state_.credentials) if(c.credentialId==id) return &c;
  return nullptr;
}
bool Engine::binding(const Credential& c) const {
  return state_.policy.bindingMode==BindingMode::Disabled || std::find(c.allowedDeviceIds.begin(),c.allowedDeviceIds.end(),deviceId_)!=c.allowedDeviceIds.end();
}
bool Engine::revoked(const Credential& c) const {
  return c.status==Status::Revoked || std::find(state_.revoked.begin(),state_.revoked.end(),c.credentialId)!=state_.revoked.end();
}
void Engine::emit(const char* event,const char* reason,uint64_t now) {
  if(sink_) sink_({now,event,deviceId_,attached_,reason});
}
void Engine::invalidate(Decision d,const char* reason,SessionState state) {
  session_={}; sessionState_=state; pending_={}; hasPending_=false; denial_=d; reason_=reason;
}
bool Engine::deny(Decision d,const char* reason,const char* event,uint64_t now) {
  invalidate(d,reason,(d==Decision::DENY_CREDENTIAL_REVOKED || d==Decision::DENY_DEVICE_REVOKED) ? SessionState::REVOKED : SessionState::NONE);
  emit(event,reason,now); tick(now,pressed_); return false;
}
void Engine::fault(uint64_t now,const char* reason) {
  healthy_=false; invalidate(Decision::DENY_SYSTEM_FAILURE,reason); emit("SYSTEM_FAULT",reason,now); tick(now,pressed_);
}
bool Engine::persist(const PersistentState& state,uint64_t now) {
  if(!store_.save(state)) { fault(now,"SECURE_STORAGE_ERROR"); return false; }
  state_=state; return true;
}
bool Engine::beginChallenge(Challenge& out,uint64_t now) {
  tick(now,pressed_);
  if(!healthy_ || controlFault_ || state_.deviceStatus!=Status::Active) return false;
  invalidate(Decision::DENY_REAUTHENTICATION_REQUIRED,"REAUTHENTICATION_REQUIRED");
  if(now>std::numeric_limits<uint64_t>::max()-state_.policy.challengeTimeoutMs ||
      !crypto::random(pending_.nonce,32) || !crypto::random(pending_.sessionContext,16)) { fault(now,"RNG_OR_TIME_FAILURE"); return false; }
  pending_.deviceId=deviceId_; pending_.policyVersion=state_.policy.version;
  challengeDeadline_=now+state_.policy.challengeTimeoutMs; hasPending_=true; out=pending_;
  emit("AUTH_CHALLENGE_CREATED","",now); tick(now,pressed_); return true;
}
bool Engine::validateResponse(const Response& r,uint64_t now) {
  tick(now,pressed_); bool pending=hasPending_; hasPending_=false;
  if(!healthy_ || controlFault_) return false;
  if(state_.deviceStatus!=Status::Active || state_.policy.mode==Mode::REVOKED) return deny(Decision::DENY_DEVICE_REVOKED,"DEVICE_REVOKED","DEVICE_REVOKED",now);
  if(!pending || now>=challengeDeadline_) return deny(Decision::DENY_REPLAY,"REPLAY_OR_INVALID_MAC","REPLAY_DETECTED",now);
  if(!validIdentifier(r.credentialId) || !validIdentifier(r.keyId) || !r.counter || r.mac.size()!=32)
    return deny(Decision::DENY_AUTH_FAILED,"MALFORMED_RESPONSE","AUTH_MAC_FAILED",now);
  attached_=r.credentialId; auto* c=credential(attached_);
  if(!c) return deny(Decision::DENY_AUTH_FAILED,"UNKNOWN_CREDENTIAL","AUTH_FAILED",now);
  if(revoked(*c)) return deny(Decision::DENY_CREDENTIAL_REVOKED,"CREDENTIAL_REVOKED","CREDENTIAL_REVOKED",now);
  if(c->status!=Status::Active || c->keyId!=r.keyId) return deny(Decision::DENY_AUTH_FAILED,"CREDENTIAL_DISABLED_OR_KEY_ID","AUTH_FAILED",now);
  if(r.counter<c->counter) return deny(Decision::DENY_COUNTER_ROLLBACK,"COUNTER_ROLLBACK","COUNTER_ROLLBACK",now);
  if(r.counter==c->counter) return deny(Decision::DENY_REPLAY,"REPLAY_OR_INVALID_MAC","REPLAY_DETECTED",now);
  if(!binding(*c)) return deny(Decision::DENY_BINDING,"DEVICE_BINDING_MISMATCH","BINDING_FAILED",now);
  auto result=keys_.verifyCredential(*c,transcript(pending_,r),r.mac);
  if(result==Verification::ERROR) { fault(now,"CREDENTIAL_VERIFICATION_ERROR"); return false; }
  if(result!=Verification::VALID) return deny(Decision::DENY_AUTH_FAILED,"REPLAY_OR_INVALID_MAC","AUTH_MAC_FAILED",now);
  auto state=state_; for(auto& entry:state.credentials) if(entry.credentialId==attached_) entry.counter=r.counter;
  std::string operatorId=c->operatorId;
  if(!persist(state,now)) return false;
  if(now>std::numeric_limits<uint64_t>::max()-state_.policy.sessionTimeoutMs || !crypto::random(session_.sessionId,32)) {
    fault(now,"RNG_OR_TIME_FAILURE"); return false;
  }
  session_.credentialId=attached_; session_.operatorId=operatorId; session_.deviceId=deviceId_;
  session_.createdAt=now; session_.expiresAt=now+state_.policy.sessionTimeoutMs; session_.policyVersion=state_.policy.version;
  sessionState_=SessionState::ACTIVE; reason_="SUCCESS"; denial_=Decision::DENY_PTT_RELEASED;
  emit("AUTH_SUCCESS","",now); emit("SESSION_CREATED","",now); tick(now,pressed_); return true;
}
bool Engine::authenticate(CredentialTransport& transport,uint64_t now) {
  Challenge c; Response r; if(!beginChallenge(c,now)) return false;
  if(!transport.respond(c,r)) { fault(now,"NFC_READER_OR_CREDENTIAL_FAILURE"); return false; }
  return validateResponse(r,now); // Synchronous mock only; hardware uses refreshed time at completion.
}
SecurityContext Engine::context(uint64_t now,bool pressed) const {
  SecurityContext x; const auto* c=credential(attached_); bool active=sessionState_==SessionState::ACTIVE;
  x.systemHealthy=healthy_; x.deviceStatus=state_.deviceStatus;
  x.credentialStatus=c ? (revoked(*c) ? Status::Revoked : c->status) : Status::Disabled;
  x.credentialPresent=c; x.credentialAuthenticated=active; x.bindingValid=c && binding(*c); x.counterFresh=active;
  x.sessionState=sessionState_; x.sessionBound=active && c && session_.credentialId==attached_ && session_.operatorId==c->operatorId &&
      session_.deviceId==deviceId_ && session_.policyVersion==state_.policy.version;
  x.sessionDeadlineMs=session_.expiresAt; x.nowMs=now; x.pttPressed=pressed; x.mode=state_.policy.mode;
  x.emergencyEnabled=state_.policy.emergencyEnabled; x.denial=denial_; return x;
}
Decision Engine::tick(uint64_t now,bool pressed) {
  pressed_=pressed;
  if(now<lastNow_ && healthy_) { healthy_=false; invalidate(Decision::DENY_SYSTEM_FAILURE,"CLOCK_ROLLBACK"); emit("SYSTEM_FAULT","CLOCK_ROLLBACK",now); }
  lastNow_=now;
  if(sessionState_==SessionState::ACTIVE && now>=session_.expiresAt) {
    invalidate(Decision::DENY_SESSION_EXPIRED,"SESSION_EXPIRED",SessionState::EXPIRED); emit("SESSION_EXPIRED","SESSION_EXPIRED",now);
  }
  Decision d=evaluate(context(now,pressed));
  if(d!=lastDecision_ && (pressed || lastDecision_==Decision::ALLOW_TX)) emit(d==Decision::ALLOW_TX ? "TX_ALLOWED" : "TX_DENIED",decisionName(d),now);
  lastDecision_=d; return d;
}
bool Engine::sessionMatches(const Bytes& id,const std::string& device,const std::string& credentialId,uint64_t now) {
  tick(now,pressed_);
  return sessionState_==SessionState::ACTIVE && crypto::equal(id,session_.sessionId) && device==deviceId_ &&
      session_.deviceId==device && credentialId==attached_ && session_.credentialId==credentialId;
}
void Engine::detach(uint64_t now) { invalidate(Decision::DENY_NO_CREDENTIAL,"NO_CREDENTIAL"); emit("CREDENTIAL_REMOVED","",now); attached_.clear(); tick(now,pressed_); }
bool Engine::rollback(uint64_t now) { controlFault_=true; return deny(Decision::DENY_POLICY_ROLLBACK,"REJECT_ROLLBACK","POLICY_ROLLBACK_DETECTED",now); }
bool Engine::updatePolicy(const Policy& policy,uint64_t now) {
  if(!validPolicy(policy)) { fault(now,"INVALID_POLICY"); return false; }
  if(policy.version<=state_.policy.version) return rollback(now);
  auto state=state_; state.policy=policy; if(!persist(state,now)) return false;
  invalidate(Decision::DENY_REAUTHENTICATION_REQUIRED,"REAUTHENTICATION_REQUIRED"); tick(now,pressed_); return true;
}
bool Engine::updateCredential(const Credential& next,uint64_t now) {
  if(!validCredential(next)) { fault(now,"INVALID_CREDENTIAL_METADATA"); return false; }
  const auto* old=credential(next.credentialId);
  if(old && next.metadataVersion<=old->metadataVersion) return rollback(now);
  for(const auto& c:state_.credentials) if(c.credentialId!=next.credentialId && c.keyId==next.keyId) { fault(now,"DUPLICATE_KEY_ID"); return false; }
  Credential updated=next; if(old) updated.counter=std::max(next.counter,old->counter);
  auto state=state_; bool found=false;
  for(auto& c:state.credentials) if(c.credentialId==next.credentialId) { c=updated; found=true; }
  if(!found) { if(state.credentials.size()>=32) { fault(now,"METADATA_CAPACITY"); return false; } state.credentials.push_back(updated); }
  if(!persist(state,now)) return false;
  if(attached_==next.credentialId) invalidate(Decision::DENY_REAUTHENTICATION_REQUIRED,"REAUTHENTICATION_REQUIRED");
  tick(now,pressed_); return true;
}
bool Engine::applyRevocationList(const RevocationList& crl,uint64_t now) {
  Bytes payload=revocationPayload(crl);
  if(payload.empty() || !crypto::ed25519Verify(publicKey_,payload,crl.signature)) {
    emit("REVOCATION_SIGNATURE_FAILED","INVALID_SIGNATURE",now); fault(now,"INVALID_REVOCATION_SIGNATURE"); return false;
  }
  if(crl.version<=state_.crl.version) return rollback(now);
  auto state=state_; state.crl=crl;
  for(const auto& e:crl.entries) if(std::find(state.revoked.begin(),state.revoked.end(),e.credentialId)==state.revoked.end()) state.revoked.push_back(e.credentialId);
  if(state.revoked.size()>256) { fault(now,"REVOCATION_CAPACITY"); return false; }
  if(!persist(state,now)) return false;
  emit("REVOCATION_LIST_UPDATED","",now); hasPending_=false;
  if(auto* c=credential(attached_)) if(revoked(*c)) deny(Decision::DENY_CREDENTIAL_REVOKED,"CREDENTIAL_REVOKED","CREDENTIAL_REVOKED",now);
  tick(now,pressed_); return true;
}
bool Engine::setDeviceStatus(Status status,uint64_t now) {
  if(static_cast<uint8_t>(status)>2) { fault(now,"INVALID_DEVICE_STATUS"); return false; }
  auto state=state_; state.deviceStatus=status; if(!persist(state,now)) return false;
  invalidate(status==Status::Active ? Decision::DENY_REAUTHENTICATION_REQUIRED : Decision::DENY_DEVICE_REVOKED,
      status==Status::Active ? "REAUTHENTICATION_REQUIRED" : "DEVICE_REVOKED",status==Status::Active ? SessionState::NONE : SessionState::REVOKED);
  emit(status==Status::Active ? "DEVICE_RESTORED" : "DEVICE_REVOKED",reason_.c_str(),now); tick(now,pressed_); return true;
}
} }
