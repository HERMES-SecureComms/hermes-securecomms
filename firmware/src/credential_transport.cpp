#include "credential_transport.h"
#include <SPI.h>
namespace hermes {
PN532Transport::PN532Transport() : reader_(config::PIN_PN532_SS, &SPI) {}
bool PN532Transport::begin() {
  operational_ = false;
  SPI.begin(config::PIN_PN532_SCK, config::PIN_PN532_MISO, config::PIN_PN532_MOSI, config::PIN_PN532_SS);
  reader_.begin();
  const uint32_t version = reader_.getFirmwareVersion();
  if (!version) { Serial.println(F("[HERMES] PN532 ERROR - FIRMWARE NOT DETECTED")); return false; }
  if (!reader_.SAMConfig()) { Serial.println(F("[HERMES] PN532 ERROR - SAM CONFIGURATION FAILED")); return false; }
  operational_ = true;
  Serial.println(F("[HERMES] PN532 detected"));
  Serial.print(F("[HERMES] PN532 firmware: "));
  Serial.print((version >> 16) & 0xFF, DEC); Serial.print('.'); Serial.println((version >> 8) & 0xFF, DEC);
  Serial.println(F("[HERMES] Waiting for credential"));
  return true;
}
bool PN532Transport::readUid(uint8_t* uid, uint8_t& length) {
  length = 0;
  return operational_ && reader_.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &length, config::NFC_POLL_TIMEOUT_MS)
      && length > 0 && length <= config::NFC_UID_MAX_LENGTH;
}
bool PN532Transport::respond(const security::Challenge&, security::Response&) {
  // A plain ISO14443 UID tag has no HMAC credential application. A selected secure
  // card's mutual-auth/APDU driver must implement this adapter. Never synthesize
  // a MAC from a UID, and never treat a successful UID read as secure response.
  return false;
}
}
