#include "hermes_security.h"
#include <algorithm>
#include <set>

namespace hermes { namespace security {
namespace {
struct Writer {
  Bytes b;
  void number(uint64_t n, size_t size) { for (size_t i=size; i>0; --i) b.push_back(static_cast<uint8_t>(n >> ((i-1)*8))); }
  void raw(const Bytes& v) { b.insert(b.end(),v.begin(),v.end()); }
  void text(const std::string& s) { number(s.size(),2); b.insert(b.end(),s.begin(),s.end()); }
  void blob(const Bytes& v) { number(v.size(),2); raw(v); }
  template<size_t N> void domain(const char (&s)[N]) { b.insert(b.end(),s,s+N); }
};
struct Reader {
  const Bytes& b; size_t p=0; bool ok=true;
  uint64_t number(size_t size) {
    if (!ok || size>b.size()-p) { ok=false; return 0; }
    uint64_t n=0; while(size--) n=(n<<8)|b[p++]; return n;
  }
  Bytes blob(size_t max=65535) {
    size_t size=number(2);
    if (!ok || size>max || size>b.size()-p) { ok=false; return {}; }
    Bytes out(b.begin()+p,b.begin()+p+size); p+=size; return out;
  }
  std::string text(size_t max=64) { auto v=blob(max); return std::string(v.begin(),v.end()); }
  size_t count(size_t max) { size_t n=number(2); if(n>max) { ok=false; return 0; } return n; }
};
void policyWrite(Writer& w,const Policy& p) {
  w.number(p.version,8); w.number(p.sessionTimeoutMs,8); w.number(p.challengeTimeoutMs,8);
  w.number(static_cast<uint8_t>(p.bindingMode),1); w.number(static_cast<uint8_t>(p.mode),1); w.number(p.emergencyEnabled,1);
}
void credentialWrite(Writer& w,const Credential& c) {
  w.text(c.credentialId); w.text(c.operatorId); w.text(c.keyId); w.number(static_cast<uint8_t>(c.status),1);
  w.number(c.counter,8); w.number(c.metadataVersion,8); w.number(c.allowedDeviceIds.size(),2);
  for(const auto& d:c.allowedDeviceIds) w.text(d);
}
bool reasonValid(const std::string& s) {
  return !s.empty() && s.size()<=128 && std::all_of(s.begin(),s.end(),[](char c){return c>=32 && c<=126;});
}
}
bool validIdentifier(const std::string& s) {
  return !s.empty() && s.size()<=64 && std::all_of(s.begin(),s.end(),[](char c){
    return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.';
  });
}
bool validPolicy(const Policy& p) {
  return p.version && p.sessionTimeoutMs && p.challengeTimeoutMs &&
      (p.bindingMode==BindingMode::Disabled || p.bindingMode==BindingMode::AllowList) &&
      (p.mode==Mode::NORMAL || p.mode==Mode::DEGRADED || p.mode==Mode::REVOKED);
}
bool validCredential(const Credential& c) {
  std::set<std::string> ids(c.allowedDeviceIds.begin(),c.allowedDeviceIds.end());
  return validIdentifier(c.credentialId) && validIdentifier(c.operatorId) && validIdentifier(c.keyId) &&
      c.metadataVersion && c.allowedDeviceIds.size()<=16 && ids.size()==c.allowedDeviceIds.size() &&
      (c.status==Status::Active || c.status==Status::Revoked || c.status==Status::Disabled) &&
      std::all_of(ids.begin(),ids.end(),validIdentifier);
}
Bytes transcript(const Challenge& c,const Response& r) {
  if(c.protocolVersion!=2 || !validIdentifier(c.deviceId) || c.nonce.size()!=32 || c.sessionContext.size()!=16 ||
     !c.policyVersion || !validIdentifier(r.credentialId) || !validIdentifier(r.keyId) || !r.counter) return {};
  Writer w; w.domain("HERMES-AUTH-v2"); w.number(c.protocolVersion,2); w.text(c.deviceId); w.raw(c.nonce);
  w.number(r.counter,8); w.number(c.policyVersion,8); w.raw(c.sessionContext); w.text(r.credentialId); w.text(r.keyId);
  return w.b;
}
Bytes revocationPayload(const RevocationList& crl) {
  if(!crl.version || crl.entries.size()>256) return {};
  std::set<std::string> seen;
  Writer w; w.domain("HERMES-CRL-v2"); w.number(crl.version,8); w.number(crl.issuedAt,8); w.number(crl.entries.size(),2);
  for(const auto& e:crl.entries) {
    if(!validIdentifier(e.credentialId) || !seen.insert(e.credentialId).second || e.revokedAt>crl.issuedAt || !reasonValid(e.reason)) return {};
    w.text(e.credentialId); w.number(e.revokedAt,8); w.text(e.reason);
  }
  return w.b;
}
Bytes encodeState(const PersistentState& s) {
  Writer w; w.text("HERMES-STATE-v2"); w.text(s.deviceId); policyWrite(w,s.policy);
  w.number(static_cast<uint8_t>(s.deviceStatus),1); w.number(s.credentials.size(),2);
  for(const auto& c:s.credentials) credentialWrite(w,c);
  w.number(s.crl.version,8); w.number(s.crl.issuedAt,8); w.number(s.crl.entries.size(),2);
  for(const auto& e:s.crl.entries) { w.text(e.credentialId); w.number(e.revokedAt,8); w.text(e.reason); }
  w.blob(s.crl.signature); w.number(s.revoked.size(),2); for(const auto& id:s.revoked) w.text(id);
  return w.b;
}
bool decodeState(const Bytes& bytes,PersistentState& out) {
  if(bytes.size()>65535) return false;
  Reader r{bytes}; PersistentState s;
  if(r.text()!="HERMES-STATE-v2") return false;
  s.deviceId=r.text(); s.policy.version=r.number(8); s.policy.sessionTimeoutMs=r.number(8); s.policy.challengeTimeoutMs=r.number(8);
  s.policy.bindingMode=static_cast<BindingMode>(r.number(1)); s.policy.mode=static_cast<Mode>(r.number(1));
  uint8_t emergency=r.number(1); if(emergency>1) return false; s.policy.emergencyEnabled=emergency;
  s.deviceStatus=static_cast<Status>(r.number(1));
  if(!validIdentifier(s.deviceId) || !validPolicy(s.policy) || static_cast<uint8_t>(s.deviceStatus)>2) return false;
  size_t count=r.count(32); std::set<std::string> seen,keys;
  for(size_t i=0;i<count;++i) {
    Credential c; c.credentialId=r.text(); c.operatorId=r.text(); c.keyId=r.text(); c.status=static_cast<Status>(r.number(1));
    c.counter=r.number(8); c.metadataVersion=r.number(8); size_t devices=r.count(16);
    for(size_t j=0;j<devices;++j) c.allowedDeviceIds.push_back(r.text());
    if(!validCredential(c) || !seen.insert(c.credentialId).second || !keys.insert(c.keyId).second) return false;
    s.credentials.push_back(c);
  }
  s.crl.version=r.number(8); s.crl.issuedAt=r.number(8); count=r.count(256);
  for(size_t i=0;i<count;++i) {
    RevocationEntry e; e.credentialId=r.text(); e.revokedAt=r.number(8); e.reason=r.text(128); s.crl.entries.push_back(e);
  }
  s.crl.signature=r.blob(64); count=r.count(256); seen.clear();
  for(size_t i=0;i<count;++i) { auto id=r.text(); if(!validIdentifier(id) || !seen.insert(id).second) return false; s.revoked.push_back(id); }
  if(s.crl.version) {
    if(revocationPayload(s.crl).empty() || s.crl.signature.size()!=64) return false;
    for(const auto& e:s.crl.entries) if(!seen.count(e.credentialId)) return false;
  } else if(!s.crl.entries.empty() || !s.crl.signature.empty() || s.crl.issuedAt) return false;
  if(!r.ok || r.p!=bytes.size()) return false;
  out=s; return true;
}
} }
