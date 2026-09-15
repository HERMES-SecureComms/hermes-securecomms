#include "nfc_auth.h"

#include <SPI.h>

#include "credentials.h"

namespace hermes {

static_assert(credentials::AUTHORIZED_UID_LENGTH <= config::NFC_UID_MAX_LENGTH,
              "Configured NFC UID is longer than the supported PN532 UID buffer");

NfcAuth::NfcAuth()
    : transport_(),
      operational_(false),
      authorized_(false),
      credentialObserved_(false),
      hasAuthorizedTimestamp_(false),
      lastAuthorizedSeenAtMs_(0),
      lastCredentialSeenAtMs_(0),
      lastUid_{0},
      lastUidLength_(0) {}

bool NfcAuth::begin() {
  operational_ = false;
  authorized_ = false;
  credentialObserved_ = false;
  hasAuthorizedTimestamp_ = false;
  lastUidLength_ = 0;

  operational_ = transport_.begin();
  return operational_;
}

void NfcAuth::update(uint32_t nowMs) {
  if (!operational_) {
    authorized_ = false;
    return;
  }

  uint8_t uid[config::NFC_UID_MAX_LENGTH] = {0};
  uint8_t uidLength = 0;
  const bool detected = transport_.readUid(uid, uidLength);

  if (detected && uidLength > 0 && uidLength <= config::NFC_UID_MAX_LENGTH) {
    const bool isNewObservation =
        !credentialObserved_ || !uidMatchesLastObserved(uid, uidLength);

    credentialObserved_ = true;
    lastCredentialSeenAtMs_ = nowMs;

    if (isNewObservation) {
      rememberUid(uid, uidLength);
      printUid(uid, uidLength);
    }

    if (uidMatchesAuthorized(uid, uidLength)) {
      lastAuthorizedSeenAtMs_ = nowMs;
      hasAuthorizedTimestamp_ = true;
      if (!authorized_) {
        authorized_ = true;
        Serial.println(F("[HERMES] AUTH SUCCESS"));
      }
      return;
    }

    const bool wasAuthorized = authorized_;
    authorized_ = false;
    hasAuthorizedTimestamp_ = false;
    if (isNewObservation || wasAuthorized) {
      Serial.println(F("[HERMES] AUTH FAILED"));
    }
    return;
  }

  if (credentialObserved_ &&
      static_cast<uint32_t>(nowMs - lastCredentialSeenAtMs_) >=
          config::AUTH_GRACE_PERIOD_MS) {
    credentialObserved_ = false;
    lastUidLength_ = 0;
  }

  if (authorized_ && hasAuthorizedTimestamp_ &&
      static_cast<uint32_t>(nowMs - lastAuthorizedSeenAtMs_) >=
          config::AUTH_GRACE_PERIOD_MS) {
    authorized_ = false;
    hasAuthorizedTimestamp_ = false;
    Serial.println(F("[HERMES] CREDENTIAL LOST"));
  }
}

bool NfcAuth::isAuthorized() const { return operational_ && authorized_; }

bool NfcAuth::isOperational() const { return operational_; }

bool NfcAuth::uidMatchesAuthorized(const uint8_t* uid,
                                   uint8_t uidLength) const {
  if (uidLength != credentials::AUTHORIZED_UID_LENGTH) {
    return false;
  }

  for (uint8_t i = 0; i < uidLength; ++i) {
    if (uid[i] != credentials::AUTHORIZED_UID[i]) {
      return false;
    }
  }
  return true;
}

bool NfcAuth::uidMatchesLastObserved(const uint8_t* uid,
                                     uint8_t uidLength) const {
  if (uidLength != lastUidLength_) {
    return false;
  }
  for (uint8_t i = 0; i < uidLength; ++i) {
    if (uid[i] != lastUid_[i]) {
      return false;
    }
  }
  return true;
}

void NfcAuth::rememberUid(const uint8_t* uid, uint8_t uidLength) {
  lastUidLength_ = uidLength;
  for (uint8_t i = 0; i < uidLength; ++i) {
    lastUid_[i] = uid[i];
  }
}

void NfcAuth::printUid(const uint8_t* uid, uint8_t uidLength) const {
  Serial.print(F("[HERMES] NFC UID:"));
  for (uint8_t i = 0; i < uidLength; ++i) {
    Serial.print(' ');
    if (uid[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(uid[i], HEX);
  }
  Serial.println();
}

}  // namespace hermes

