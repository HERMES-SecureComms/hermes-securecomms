#include "config.h"
#ifdef HERMES_AUTH_MODE_SECURE
#include <Arduino.h>
#include <esp_timer.h>
#include "credential_transport.h"
#include "nvs_state_store.h"
#include "ptt_controller.h"
#ifdef HERMES_SECURE_MOCK
#include "../test/fixtures/mock_provisioning.h"
#endif

namespace {
using namespace hermes;
PttController ptt;
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }
void trace(const security::Event& e) {
  // Event/identifier/reason vocabulary contains no quotes or control characters.
  Serial.printf("{\"timestamp_ms\":%llu,\"event\":\"%s\",\"device_id\":\"%s\",\"credential_id\":\"%s\",\"reason\":\"%s\"}\n",
    static_cast<unsigned long long>(e.timestampMs),e.event.c_str(),e.deviceId.c_str(),e.credentialId.c_str(),e.reason.c_str());
}
#ifdef HERMES_SECURE_MOCK
security::MemoryStateStore stateStore, credentialStore;
security::SoftwareKeyStore keys;
security::Engine engine("HPTT-001",keys,stateStore,test_fixture::publicKey(),trace);
security::MockCredentialTransport transport(test_fixture::state().credentials[0],test_fixture::key(),credentialStore);
#else
NvsStateStore stateStore;
security::SecureElementKeyStore keys;
// No private key or default production trust anchor is provisioned by this build.
// Empty anchor + missing NVS => fail closed. Replace the adapter during hardware provisioning.
security::Engine engine("HPTT-001",keys,stateStore,{},trace);
PN532Transport transport;
bool readerReady = false;
#endif
}
void setup() {
  ptt.begin();
  Serial.begin(hermes::config::SERIAL_BAUD);
#ifdef HERMES_SECURE_MOCK
  stateStore.state=test_fixture::state();
  stateStore.state.policy.sessionTimeoutMs=config::SESSION_TIMEOUT_MS;
  stateStore.state.policy.challengeTimeoutMs=config::CHALLENGE_TIMEOUT_MS;
  credentialStore.state=test_fixture::state();
  keys.add("GLOVE-001","KEY-GLOVE-001-v1",test_fixture::key());
  Serial.println(F("[HERMES] SOFTWARE SECURITY PoC ONLY: a=authenticate d=detach f=fault r=recover"));
#else
  readerReady=transport.begin();
  if(!readerReady) { engine.fault(nowMs(),"NFC_READER_FAILURE"); ptt.latchPermanentFault(); return; }
#endif
  if(!engine.begin(nowMs())) {
    ptt.latchPermanentFault();
    return;
  }
  ptt.setSystemReady();
}
void loop() {
  ptt.updateSecurity(engine,nowMs());
#ifdef HERMES_SECURE_MOCK
  if(Serial.available()) {
    char command=Serial.read();
    if(command=='a') {
      security::Challenge challenge; security::Response response;
      if(engine.beginChallenge(challenge,nowMs())) {
        if(transport.respond(challenge,response)) engine.validateResponse(response,nowMs());
        else engine.fault(nowMs(),"CREDENTIAL_FAILURE");
      }
    } else if(command=='d') engine.detach(nowMs());
    else if(command=='f') engine.fault(nowMs());
    else if(command=='r') engine.recover(nowMs());
  }
#else
  // Authentication is unavailable until a real secure-credential adapter and
  // key provisioning exist. UID discovery cannot grant TX in the default build.
  if(readerReady && engine.healthy()) {
    uint8_t uid[config::NFC_UID_MAX_LENGTH]={0}; uint8_t length=0;
    if(transport.readUid(uid,length)) {
      security::Challenge challenge; security::Response response;
      if(engine.beginChallenge(challenge,nowMs())) {
        if(transport.respond(challenge,response)) engine.validateResponse(response,nowMs());
        else engine.fault(nowMs(),"SECURE_CREDENTIAL_TRANSPORT_UNAVAILABLE");
      }
    } else engine.detach(nowMs());
  }
#endif
  ptt.updateSecurity(engine,nowMs());
  delay(config::MAIN_LOOP_DELAY_MS);
}
#endif
