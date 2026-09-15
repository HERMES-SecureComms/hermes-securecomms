#include "config.h"
#ifdef HERMES_AUTH_MODE_UID_POC
#include <Arduino.h>

#include "config.h"
#include "nfc_auth.h"
#include "ptt_controller.h"

namespace {

hermes::NfcAuth nfcAuth;
hermes::PttController pttController;

}  // namespace

void setup() {
  // Establish the safe outputs before serial or peripheral initialization.
  pttController.begin();

  Serial.begin(hermes::config::SERIAL_BAUD);
  Serial.println();
  Serial.println(F("[HERMES] Booting..."));
  Serial.println(F("[HERMES] TX DISABLED"));

  if (!nfcAuth.begin()) {
    pttController.latchPermanentFault();
    Serial.println(F("[HERMES] PN532 FAILURE - TX PERMANENTLY DISABLED"));
    return;
  }

  pttController.setSystemReady();
  Serial.println(F("[HERMES] System ready"));
}

void loop() {
  if (!nfcAuth.isOperational()) {
    pttController.update(false, millis());
    delay(hermes::config::MAIN_LOOP_DELAY_MS);
    return;
  }

  nfcAuth.update(millis());
  // Refresh millis() because an NFC poll can wait for NFC_POLL_TIMEOUT_MS.
  pttController.update(nfcAuth.isAuthorized(), millis());
  delay(hermes::config::MAIN_LOOP_DELAY_MS);
}


#endif
