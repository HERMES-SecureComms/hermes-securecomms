#pragma once

#include "credential_transport.h"
#include <Arduino.h>

#include "config.h"

namespace hermes {

class NfcAuth {
 public:
  NfcAuth();

  bool begin();
  void update(uint32_t nowMs);

  bool isAuthorized() const;
  bool isOperational() const;

 private:
  bool uidMatchesAuthorized(const uint8_t* uid, uint8_t uidLength) const;
  bool uidMatchesLastObserved(const uint8_t* uid, uint8_t uidLength) const;
  void rememberUid(const uint8_t* uid, uint8_t uidLength);
  void printUid(const uint8_t* uid, uint8_t uidLength) const;

  PN532Transport transport_;
  bool operational_;
  bool authorized_;
  bool credentialObserved_;
  bool hasAuthorizedTimestamp_;
  uint32_t lastAuthorizedSeenAtMs_;
  uint32_t lastCredentialSeenAtMs_;
  uint8_t lastUid_[config::NFC_UID_MAX_LENGTH];
  uint8_t lastUidLength_;
};

}  // namespace hermes

