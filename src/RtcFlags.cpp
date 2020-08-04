#include "RtcFlags.h"

uint16_t RtcFlags::getFlags() {
  uint32_t rtcData[2];

  if (ESP.rtcUserMemoryRead(0, rtcData, sizeof(rtcData))) {
    if ((rtcData[0] == RTC_SIGN) && (((~rtcData[1]) >> 16) == (rtcData[1] & 0xFFFF)))
      return rtcData[1] & 0xFFFF;
  }
  return 0;
}

bool RtcFlags::getFlag(uint8_t flag) {
  return (getFlags() & (1 << flag)) != 0;
}

bool RtcFlags::setFlags(uint16_t flags) {
  uint32_t rtcData[2];

  rtcData[0] = RTC_SIGN;
  rtcData[1] = ((~flags) << 16) | flags;
  return ESP.rtcUserMemoryWrite(0, rtcData, sizeof(rtcData));
}

bool RtcFlags::setFlag(uint8_t flag) {
  return setFlags(getFlags() | (1 << flag));
}

bool RtcFlags::clearFlag(uint8_t flag) {
  return setFlags(getFlags() & ~(1 << flag));
}
