#include "ptt_controller.h"

#include "config.h"

namespace hermes {

PttController::PttController()
    : systemReady_(false),
      permanentFault_(false),
      rawPressed_(false),
      stablePressed_(false),
      txEnabled_(false),
      denialLogged_(false),
      rawStateChangedAtMs_(0) {}

void PttController::begin() {
  // Load safe output latches before changing the pins to output mode.
  digitalWrite(config::PIN_TX_GATE, LOW);
  digitalWrite(config::PIN_LED_ALLOW, LOW);
  digitalWrite(config::PIN_LED_DENY, LOW);
  pinMode(config::PIN_TX_GATE, OUTPUT);
  pinMode(config::PIN_LED_ALLOW, OUTPUT);
  pinMode(config::PIN_LED_DENY, OUTPUT);

  pinMode(config::PIN_PTT_BUTTON, INPUT_PULLUP);
  rawPressed_ = readButtonPressed();
  stablePressed_ = false;
  rawStateChangedAtMs_ = millis();

  systemReady_ = false;
  permanentFault_ = false;
  txEnabled_ = false;
  denialLogged_ = false;
}

void PttController::setSystemReady() {
  if (!permanentFault_) {
    systemReady_ = true;
  }
}

void PttController::latchPermanentFault() {
  permanentFault_ = true;
  systemReady_ = false;
  digitalWrite(config::PIN_TX_GATE, LOW);
  digitalWrite(config::PIN_LED_ALLOW, LOW);
  digitalWrite(config::PIN_LED_DENY, HIGH);
  txEnabled_ = false;
}

void PttController::updateButton(uint32_t nowMs) {
  const bool currentRawPressed = readButtonPressed();
  if (currentRawPressed != rawPressed_) {
    rawPressed_ = currentRawPressed;
    rawStateChangedAtMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ &&
      static_cast<uint32_t>(nowMs - rawStateChangedAtMs_) >=
          config::PTT_DEBOUNCE_MS) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      logTimedEvent(F("PTT PRESSED"), nowMs);
    } else {
      logTimedEvent(F("PTT RELEASED"), nowMs);
      denialLogged_ = false;
    }
  }

}

void PttController::update(bool authenticated, uint32_t nowMs) {
  updateButton(nowMs);
  applyAuthorization(authenticated, nowMs);
}

void PttController::updateSecurity(security::Engine& engine, uint64_t nowMs) {
  updateButton(static_cast<uint32_t>(nowMs));
  const auto decision = engine.tick(nowMs, stablePressed_);
  applyAuthorization(decision == security::Decision::ALLOW_TX, static_cast<uint32_t>(nowMs));
}

void PttController::applyAuthorization(bool authenticated, uint32_t nowMs) {
  const bool shouldEnableTx =
      systemReady_ && !permanentFault_ && authenticated && stablePressed_;
  setTxEnabled(shouldEnableTx, nowMs);

  if (shouldEnableTx) {
    digitalWrite(config::PIN_LED_DENY, LOW);
    denialLogged_ = false;
  } else if (stablePressed_) {
    digitalWrite(config::PIN_LED_DENY, HIGH);
    if (!denialLogged_) {
      logTimedEvent(F("TX DENIED - NO VALID CREDENTIAL"), nowMs);
      denialLogged_ = true;
    }
  } else {
    digitalWrite(config::PIN_LED_DENY, permanentFault_ ? HIGH : LOW);
  }
}

bool PttController::isTxEnabled() const { return txEnabled_; }

bool PttController::readButtonPressed() const {
  return digitalRead(config::PIN_PTT_BUTTON) == LOW;
}

void PttController::setTxEnabled(bool enabled, uint32_t nowMs) {
  if (enabled == txEnabled_) {
    // Reassert the fail-closed output state even if an earlier write was upset.
    if (!enabled) {
      digitalWrite(config::PIN_TX_GATE, LOW);
      digitalWrite(config::PIN_LED_ALLOW, LOW);
    }
    return;
  }

  txEnabled_ = enabled;
  digitalWrite(config::PIN_TX_GATE, enabled ? HIGH : LOW);
  digitalWrite(config::PIN_LED_ALLOW, enabled ? HIGH : LOW);
  logTimedEvent(enabled ? F("TX ENABLED") : F("TX DISABLED"), nowMs);
}

void PttController::logTimedEvent(const __FlashStringHelper* event,
                                  uint32_t nowMs) const {
  Serial.print(F("[HERMES] "));
  Serial.print(event);
  Serial.print(F(" t_ms="));
  Serial.println(nowMs);
}

}  // namespace hermes

