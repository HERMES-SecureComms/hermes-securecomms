#pragma once

#include <Arduino.h>

#if defined(HERMES_AUTH_MODE_UID_POC) && defined(HERMES_AUTH_MODE_SECURE)
#error "Choose exactly one authentication mode"
#endif
#if !defined(HERMES_AUTH_MODE_UID_POC) && !defined(HERMES_AUTH_MODE_SECURE)
#define HERMES_AUTH_MODE_SECURE 1
#endif

namespace hermes {
namespace config {

// PN532 hardware SPI pins.
constexpr uint8_t PIN_PN532_SCK = 12;
constexpr uint8_t PIN_PN532_MISO = 13;
constexpr uint8_t PIN_PN532_MOSI = 11;
constexpr uint8_t PIN_PN532_SS = 10;

// User interface and simulated TX output pins.
constexpr uint8_t PIN_PTT_BUTTON = 4;
constexpr uint8_t PIN_TX_GATE = 5;
constexpr uint8_t PIN_LED_ALLOW = 6;
constexpr uint8_t PIN_LED_DENY = 7;

constexpr uint64_t SESSION_TIMEOUT_MS = 5000; // PoC experiment parameter.
constexpr uint64_t CHALLENGE_TIMEOUT_MS = 2000;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t AUTH_GRACE_PERIOD_MS = 1500;
constexpr uint32_t PTT_DEBOUNCE_MS = 30;
constexpr uint16_t NFC_POLL_TIMEOUT_MS = 50;
constexpr uint32_t MAIN_LOOP_DELAY_MS = 5;
constexpr uint8_t NFC_UID_MAX_LENGTH = 10;

}  // namespace config
}  // namespace hermes

