#pragma once

#include <Arduino.h>

class RtcFlags {
public:
  static uint16_t getFlags();
  static bool getFlag(uint8_t flag);
  static bool setFlags(uint16_t flags);
  static bool setFlag(uint8_t flag);
  static bool clearFlag(uint8_t flag);

protected:
  static const uint32_t RTC_SIGN = 0xA1B2C3D4;
};
