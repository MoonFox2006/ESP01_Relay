#ifdef ESP8266
#include <pgmspace.h>
#endif
#include "Base64.h"

static bool isBase64(char c) {
  return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '+') || (c == '/');
}

static char encodeByte(uint8_t b) {
  if (b < 26)
    return 'A' + b;
  if (b < 52)
    return 'a' + b - 26;
  if (b < 62)
    return '0' + b - 52;
  if (b == 62)
    return '+';
  if (b == 63)
    return '/';
//  return '=';
}

static uint8_t decodeByte(char c) {
  if ((c >= 'A') && (c <= 'Z'))
    return c - 'A';
  if ((c >= 'a') && (c <= 'z'))
    return c - 'a' + 26;
  if ((c >= '0') && (c <= '9'))
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
//  return 0;
}

String encodeBase64(const uint8_t *data, uint16_t size) {
  String result;
  uint32_t buffer = 0;

  if (result.reserve((size + 2) / 3 * 4)) {
    while (size >= 3) {
      buffer = 0;
      for (int8_t i = 2; i >= 0; --i) {
#ifdef ESP8266
        buffer |= pgm_read_byte(data++) << (i * 8);
#else
        buffer |= *data++ << (i * 8);
#endif
      }
      for (int8_t i = 3; i >= 0; --i) {
        result.concat(encodeByte((buffer >> (i * 6)) & 0x3F));
      }
      size -= 3;
    }
    if (size) {
#ifdef ESP8266
      buffer = pgm_read_byte(data++) << 16;
#else
      buffer = *data++ << 16;
#endif
      if (size > 1)
#ifdef ESP8266
        buffer |= pgm_read_byte(data) << 8;
#else
        buffer |= *data << 8;
#endif
      result.concat(encodeByte((buffer >> 18) & 0x3F));
      result.concat(encodeByte((buffer >> 12) & 0x3F));
      if (size > 1)
        result.concat(encodeByte((buffer >> 6) & 0x3F));
      result.concat('=');
      if (size == 1)
        result.concat('=');
    }
  }
  return result;
}

int16_t encodeBase64(Stream &stream, const uint8_t *data, uint16_t size) {
  uint16_t result = 0;
  uint32_t buffer = 0;

  while (size >= 3) {
    buffer = 0;
    for (int8_t i = 2; i >= 0; --i) {
#ifdef ESP8266
      buffer |= pgm_read_byte(data++) << (i * 8);
#else
      buffer |= *data++ << (i * 8);
#endif
    }
    for (int8_t i = 3; i >= 0; --i) {
      if (stream.print(encodeByte((buffer >> (i * 6)) & 0x3F)) != sizeof(char))
        return -1;
      result += sizeof(char);
    }
    size -= 3;
  }
  if (size) {
#ifdef ESP8266
    buffer = pgm_read_byte(data++) << 16;
#else
    buffer = *data++ << 16;
#endif
    if (size > 1)
#ifdef ESP8266
      buffer |= pgm_read_byte(data) << 8;
#else
      buffer |= *data << 8;
#endif
    if (stream.print(encodeByte((buffer >> 18) & 0x3F)) != sizeof(char))
      return -1;
    result += sizeof(char);
    if (stream.print(encodeByte((buffer >> 12) & 0x3F)) != sizeof(char))
      return -1;
    result += sizeof(char);
    if (size > 1) {
      if (stream.print(encodeByte((buffer >> 6) & 0x3F)) != sizeof(char))
        return -1;
      result += sizeof(char);
    }
    if (stream.print('=') != sizeof(char))
      return -1;
    result += sizeof(char);
    if (size == 1) {
      if (stream.print('=') != sizeof(char))
        return -1;
      result += sizeof(char);
    }
  }
  return result;
}

int16_t decodeBase64(const char *str, uint8_t *data, uint16_t maxsize) {
  uint16_t result = 0;
  uint32_t buffer = 0;
  uint8_t buflen = 0;

#ifdef ESP8266
  while (pgm_read_byte(str) && maxsize) {
#else
  while (*str && maxsize) {
#endif
#ifdef ESP8266
    if (pgm_read_byte(str) == '=')
#else
    if (*str == '=')
#endif
      break;
#ifdef ESP8266
    if (! isBase64(pgm_read_byte(str)))
#else
    if (! isBase64(*str))
#endif
      return -1;
#ifdef ESP8266
    buffer |= decodeByte(pgm_read_byte(str++)) << ((3 - buflen) * 6);
#else
    buffer |= decodeByte(*str++) << ((3 - buflen) * 6);
#endif
    if (++buflen >= 4) {
      for (int8_t i = 2; i >= 0; --i) {
        if (maxsize) {
          *data++ = buffer >> (i * 8);
          --maxsize;
          ++result;
        } else
          break;
      }
      buffer = 0;
      buflen = 0;
    }
  }
  if (maxsize && buflen) {
    *data++ = buffer >> 16;
    --maxsize;
    ++result;
    if (maxsize && (buflen > 2)) {
      *data = buffer >> 8;
//      --maxsize;
      ++result;
    }
  }
  return result;
}

int16_t decodeBase64(Stream &stream, uint8_t *data, uint16_t maxsize) {
  uint16_t result = 0;
  uint32_t buffer = 0;
  uint8_t buflen = 0;

  while (stream.available() && maxsize) {
    char c = stream.read();

    if (c == '=')
      break;
    if (! isBase64(c))
      return -1;
    buffer |= decodeByte(c) << ((3 - buflen) * 6);
    if (++buflen >= 4) {
      for (int8_t i = 2; i >= 0; --i) {
        if (maxsize) {
          *data++ = buffer >> (i * 8);
          --maxsize;
          ++result;
        } else
          break;
      }
      buffer = 0;
      buflen = 0;
    }
  }
  if (maxsize && buflen) {
    *data++ = buffer >> 16;
    --maxsize;
    ++result;
    if (maxsize && (buflen > 2)) {
      *data = buffer >> 8;
//      --maxsize;
      ++result;
    }
  }
  return result;
}
