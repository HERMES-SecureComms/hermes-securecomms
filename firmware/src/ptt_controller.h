#pragma once

#include <Arduino.h>
#include "hermes_security.h"

namespace hermes {

class PttController {
 public:
  PttController();

  void begin();
  void setSystemReady();
  void latchPermanentFault();
  void update(bool authenticated, uint32_t nowMs);

  bool isTxEnabled() const;
  void updateSecurity(security::Engine& engine, uint64_t nowMs);

 private:
  void updateButton(uint32_t nowMs);
  void applyAuthorization(bool authenticated, uint32_t nowMs);
  bool readButtonPressed() const;
  void setTxEnabled(bool enabled, uint32_t nowMs);
  void logTimedEvent(const __FlashStringHelper* event, uint32_t nowMs) const;

  bool systemReady_;
  bool permanentFault_;
  bool rawPressed_;
  bool stablePressed_;
  bool txEnabled_;
  bool denialLogged_;
  uint32_t rawStateChangedAtMs_;
};

}  // namespace hermes

