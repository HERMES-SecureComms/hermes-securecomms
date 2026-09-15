#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
struct __FlashStringHelper {};
#define F(x) reinterpret_cast<const __FlashStringHelper*>(x)
inline std::map<int,int> testPins;
inline uint32_t testMillis=0;
inline void digitalWrite(int pin,int value) { testPins[pin]=value; }
inline void pinMode(int,int) {}
inline int digitalRead(int pin) { return testPins[pin]; }
inline uint32_t millis() { return testMillis; }
struct FakeSerial { template<class T> void print(T) {} template<class T> void println(T) {} void println() {} };
inline FakeSerial Serial;
