#include "hermes_security.h"
#include "../../test/fixtures/mock_provisioning.h"
#include "../../test/fixtures/vectors.h"
#include "ptt_controller.h"
#include "config.h"
#include <iostream>
#include <set>
#include <cstdlib>
#include <limits>
using namespace hermes::security;
namespace tf=hermes::test_fixture;
static int checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<"FAILED "<<__FILE__<<":"<<__LINE__<<" "<<#x<<"\n"; std::exit(1); } } while(0)
struct Rig {
  MemoryStateStore ptt, glove;
  SoftwareKeyStore keys;
  std::vector<Event> events;
  Engine engine;
  MockCredentialTransport transport;
  Rig(std::string device="HPTT-001") : engine(device,keys,ptt,tf::publicKey(),[this](const Event& e){events.push_back(e);}),
      transport(tf::state().credentials[0],tf::key(),glove) {
    ptt.state=tf::state(); ptt.state.deviceId=device; glove.state=tf::state();
    CHECK(keys.add("GLOVE-001","KEY-GLOVE-001-v1",tf::key())); CHECK(engine.begin(0));
  }
  void active(uint64_t now=0) { CHECK(engine.authenticate(transport,now)); CHECK(engine.tick(now,true)==Decision::ALLOW_TX); }
};
static void report(const char* scenario,Decision d) {
  std::cout<<"{\"scenario\":\""<<scenario<<"\",\"decision\":\""<<decisionName(d)<<"\"}\n";
}
int main() {
  {
    Challenge c; c.deviceId="HPTT-001"; c.nonce=tf::key(); c.sessionContext=Bytes(16);
    for(size_t i=0;i<16;++i)c.sessionContext[i]=i;
    Response r{"GLOVE-001","KEY-GLOVE-001-v1",10,{}};
    CHECK(transcript(c,r)==fixtures::transcript()); Bytes mac;
    CHECK(crypto::hmacSha256(tf::key(),transcript(c,r),mac)); CHECK(mac==fixtures::mac());
    auto crl=fixtures::crl11(); CHECK(crypto::ed25519Verify(tf::publicKey(),revocationPayload(crl),crl.signature));
    crl.signature[0]^=1; CHECK(!crypto::ed25519Verify(tf::publicKey(),revocationPayload(crl),crl.signature));
    CHECK(!crypto::equal(mac,Bytes(32))); CHECK(!crypto::equal(mac,Bytes(31)));
  }
  {
    Rig r; std::set<Bytes> nonces,contexts,sessions;
    for(int i=0;i<250;++i) {
      Challenge c; Response response; CHECK(r.engine.beginChallenge(c,0)); nonces.insert(c.nonce); contexts.insert(c.sessionContext);
      CHECK(r.transport.respond(c,response)); CHECK(r.engine.validateResponse(response,0)); sessions.insert(r.engine.session().sessionId);
    }
    CHECK(nonces.size()==250 && contexts.size()==250 && sessions.size()==250);
    CHECK(r.engine.tick(0,true)==Decision::ALLOW_TX); report("secure_normal_operation",r.engine.tick(0,true));
  }
  for(bool accepted:{false,true}) {
    Rig r; Challenge a,b; Response response;
    CHECK(r.engine.beginChallenge(a,0)); CHECK(r.transport.respond(a,response));
    if(accepted) CHECK(r.engine.validateResponse(response,0));
    CHECK(r.engine.beginChallenge(b,0)); CHECK(a.nonce!=b.nonce);
    CHECK(!r.engine.validateResponse(response,0)); CHECK(r.engine.reason()=="REPLAY_OR_INVALID_MAC");
    CHECK(!r.engine.validateResponse(response,0));
    if(accepted) report("replay_attack",r.engine.tick(0,true));
  }
  {
    Rig r; r.glove.state.credentials[0].counter=9; r.active();
    r.glove.state.credentials[0].counter=8; CHECK(!r.engine.authenticate(r.transport,0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_COUNTER_ROLLBACK); report("counter_rollback",r.engine.tick(0,true));
    CHECK(r.engine.state().credentials[0].counter==10);
  }
  for(uint64_t elapsed:{4999,5000,5001}) {
    Rig r; r.active(); auto sid=r.engine.session().sessionId;
    CHECK(r.engine.sessionMatches(sid,"HPTT-001","GLOVE-001",0));
    CHECK(!r.engine.sessionMatches(sid,"HPTT-099","GLOVE-001",0));
    CHECK(!r.engine.sessionMatches(sid,"HPTT-001","GLOVE-002",0));
    CHECK(!r.engine.sessionMatches(Bytes(32),"HPTT-001","GLOVE-001",0));
    auto d=r.engine.tick(elapsed,true);
    CHECK((d==Decision::ALLOW_TX)==(elapsed<5000));
    if(elapsed>=5000) { CHECK(d==Decision::DENY_SESSION_EXPIRED); CHECK(r.engine.session().sessionId.empty()); }
    if(elapsed==5000) report("session_expired_during_tx",d);
  }
  for(uint64_t elapsed:{1999,2000,2001}) {
    Rig r; Challenge c; Response response; CHECK(r.engine.beginChallenge(c,0)); CHECK(r.transport.respond(c,response));
    CHECK(r.engine.validateResponse(response,elapsed)==(elapsed<2000));
  }
  {
    Rig r; r.active(); CHECK(r.engine.applyRevocationList(fixtures::crl11(),0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_CREDENTIAL_REVOKED); CHECK(!r.engine.authenticate(r.transport,0));
    CHECK(r.engine.sessionState()==SessionState::REVOKED); report("credential_revoked",r.engine.tick(0,true));
    CHECK(r.engine.recover(0)); CHECK(!r.engine.authenticate(r.transport,0));
  }
  for(Status status:{Status::Revoked,Status::Disabled}) {
    Rig r; auto credential=r.ptt.state.credentials[0]; credential.status=status; ++credential.metadataVersion;
    CHECK(r.engine.updateCredential(credential,0)); CHECK(!r.engine.authenticate(r.transport,0));
  }
  {
    Rig r("HPTT-099"); CHECK(!r.engine.authenticate(r.transport,0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_BINDING); report("device_binding_failure",r.engine.tick(0,true));
    Policy p; p.version=2; p.bindingMode=BindingMode::Disabled; CHECK(r.engine.updatePolicy(p,0)); r.active();
  }
  {
    Rig r; CHECK(r.engine.applyRevocationList(fixtures::crl10(),0));
    CHECK(!r.engine.applyRevocationList(fixtures::crl9(),0)); CHECK(r.engine.state().crl.version==10);
    CHECK(r.engine.tick(0,true)==Decision::DENY_POLICY_ROLLBACK); CHECK(!r.engine.authenticate(r.transport,0));
    report("revocation_list_rollback",r.engine.tick(0,true));
  }
  for(bool signature:{false,true}) {
    Rig r; r.active(); auto crl=fixtures::crl11();
    if(signature) crl.signature[0]^=1; else crl.entries[0].reason="modified";
    CHECK(!r.engine.applyRevocationList(crl,0)); CHECK(r.engine.tick(0,true)==Decision::DENY_SYSTEM_FAILURE);
    CHECK(r.engine.state().crl.version==0);
    if(!signature) report("tampered_revocation_list",r.engine.tick(0,true));
  }
  {
    Rig r; r.active(); r.engine.fault(0); CHECK(r.engine.session().sessionId.empty());
    CHECK(r.engine.tick(0,true)==Decision::DENY_SYSTEM_FAILURE); CHECK(r.engine.recover(0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_REAUTHENTICATION_REQUIRED);
    r.active(); report("system_fault_during_session",r.engine.tick(0,true));
    r.engine.detach(0); CHECK(r.engine.tick(0,true)==Decision::DENY_NO_CREDENTIAL);
  }
  {
    Rig r; r.active(); CHECK(r.engine.setDeviceStatus(Status::Revoked,0)); CHECK(!r.engine.authenticate(r.transport,0));
    CHECK(r.engine.recover(0)); CHECK(r.engine.tick(0,true)==Decision::DENY_DEVICE_REVOKED);
    CHECK(r.engine.setDeviceStatus(Status::Active,0)); CHECK(r.engine.tick(0,true)==Decision::DENY_REAUTHENTICATION_REQUIRED); r.active();
  }
  for(bool metadata:{false,true}) {
    Rig r; r.active();
    if(metadata) { auto c=r.engine.state().credentials[0]; c.metadataVersion=10; CHECK(r.engine.updateCredential(c,0)); c.metadataVersion=9; CHECK(!r.engine.updateCredential(c,0)); }
    else { Policy p; p.version=10; CHECK(r.engine.updatePolicy(p,0)); p.version=9; CHECK(!r.engine.updatePolicy(p,0)); }
    CHECK(r.engine.tick(0,true)==Decision::DENY_POLICY_ROLLBACK); CHECK(r.engine.recover(0)); CHECK(r.engine.session().sessionId.empty());
  }
  {
    Rig r; r.active(); r.ptt.failed=true; CHECK(!r.engine.authenticate(r.transport,0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_SYSTEM_FAILURE); CHECK(r.engine.session().sessionId.empty());
    CHECK(!r.engine.recover(0)); r.ptt.failed=false; CHECK(r.engine.recover(0));
    CHECK(r.engine.tick(0,true)==Decision::DENY_REAUTHENTICATION_REQUIRED);
  }
  {
    Rig r; r.active(10); CHECK(r.engine.tick(9,true)==Decision::DENY_SYSTEM_FAILURE);
    Rig overflow; CHECK(!overflow.engine.authenticate(overflow.transport,std::numeric_limits<uint64_t>::max()));
  }
  for(Mode mode:{Mode::NORMAL,Mode::DEGRADED,Mode::REVOKED}) for(bool emergency:{false,true}) {
    Rig r; Policy p; p.version=2; p.mode=mode; p.emergencyEnabled=emergency; CHECK(r.engine.updatePolicy(p,0));
    auto d=r.engine.tick(0,true); CHECK(d!=Decision::ALLOW_TX);
    if(mode==Mode::DEGRADED) CHECK(d==(emergency ? Decision::ALLOW_EMERGENCY_ONLY : Decision::DENY_DEGRADED));
    r.engine.fault(0); CHECK(r.engine.tick(0,true)==Decision::DENY_SYSTEM_FAILURE);
  }
  for(unsigned int bits=0;bits<128;++bits) {
    SecurityContext c; c.systemHealthy=bits&1; c.credentialPresent=bits&2; c.credentialAuthenticated=bits&4;
    c.bindingValid=bits&8; c.counterFresh=bits&16; c.sessionBound=bits&32; c.pttPressed=bits&64;
    c.deviceStatus=c.credentialStatus=Status::Active; c.sessionState=SessionState::ACTIVE; c.sessionDeadlineMs=5000;
    CHECK((evaluate(c)==Decision::ALLOW_TX)==(bits==127));
  }
  {
    auto original=tf::state(); auto bytes=encodeState(original); PersistentState restored;
    CHECK(decodeState(bytes,restored)); CHECK(encodeState(restored)==bytes);
    for(size_t n=0;n<bytes.size();++n) CHECK(!decodeState(Bytes(bytes.begin(),bytes.begin()+n),restored));
    bytes.push_back(0); CHECK(!decodeState(bytes,restored));
    original.policy.mode=static_cast<Mode>(99); CHECK(!decodeState(encodeState(original),restored));
    SoftwareKeyStore k; CHECK(k.add("G1","K1",tf::key())); CHECK(!k.add("G2","K2",tf::key()));
    SecureElementKeyStore se; CHECK(se.verifyCredential(tf::state().credentials[0],{}, {})==Verification::ERROR);
  }
  for(int attack=0;attack<4;++attack) {
    Rig r; Challenge c; Response response; CHECK(r.engine.beginChallenge(c,0)); CHECK(r.transport.respond(c,response));
    if(attack==0) response.mac[0]^=1;
    if(attack==1) response.keyId="WRONG-KEY";
    if(attack==2) response.credentialId="UNKNOWN";
    if(attack==3) response.mac.clear();
    CHECK(!r.engine.validateResponse(response,0)); CHECK(r.engine.state().credentials[0].counter==0);
    CHECK(r.engine.tick(0,true)==Decision::DENY_AUTH_FAILED);
  }
  // Real PttController with fake GPIO: debounce, release, permanent fault and v0.2 expiry.
  {
    using namespace hermes; testPins[config::PIN_PTT_BUTTON]=HIGH; testMillis=0;
    PttController p; p.begin(); CHECK(testPins[config::PIN_TX_GATE]==LOW); p.setSystemReady();
    testPins[config::PIN_PTT_BUTTON]=LOW; p.update(true,0); p.update(true,29); CHECK(!p.isTxEnabled());
    p.update(true,30); CHECK(p.isTxEnabled()); CHECK(testPins[config::PIN_LED_ALLOW]==HIGH);
    p.update(false,31); CHECK(!p.isTxEnabled()); p.update(true,32); CHECK(p.isTxEnabled());
    testPins[config::PIN_PTT_BUTTON]=HIGH; p.update(true,33); p.update(true,63); CHECK(!p.isTxEnabled());
    p.latchPermanentFault(); testPins[config::PIN_PTT_BUTTON]=LOW; p.update(true,64); p.update(true,94); CHECK(!p.isTxEnabled());
    Rig r; CHECK(r.engine.authenticate(r.transport,0)); testPins[config::PIN_PTT_BUTTON]=LOW;
    PttController secure; secure.begin(); secure.setSystemReady(); secure.updateSecurity(r.engine,30); CHECK(secure.isTxEnabled());
    secure.updateSecurity(r.engine,4999); CHECK(secure.isTxEnabled()); secure.updateSecurity(r.engine,5000);
    CHECK(!secure.isTxEnabled() && testPins[config::PIN_TX_GATE]==LOW);
  }
  std::cout<<"Native security / GPIO checks: "<<checks<<" PASS\n";
}
