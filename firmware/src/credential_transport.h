#pragma once
#include <Adafruit_PN532.h>
#include <Arduino.h>
#include "config.h"
#include "hermes_security.h"

namespace hermes {
// UID discovery is transport-only. No UID can authorize a v0.2 session.
class PN532Transport : public security::CredentialTransport {
 public:
  PN532Transport();
  bool begin();
  bool readUid(uint8_t* uid, uint8_t& length);
  bool operational() const { return operational_; }
  bool respond(const security::Challenge&, security::Response&) override;
 private:
  Adafruit_PN532 reader_;
  bool operational_ = false;
};
}
