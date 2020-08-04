#include <stdarg.h>
#include <stdio.h>
#include "StrUtils.h"

String printfToString(const char *fmt, ...) {
  const uint16_t MAX_SIZE = 256;

  va_list vargs;
  char msg[MAX_SIZE];

  va_start(vargs, fmt);
  vsnprintf_P(msg, sizeof(msg), fmt, vargs);
  va_end(vargs);
  return String(msg);
}

int8_t strcmp_PP(const char *s1, const char *s2) {
  char c1, c2;

  do {
    c1 = pgm_read_byte(s1++);
    c2 = pgm_read_byte(s2++);
  } while (c1 && (c1 == c2));
  return c1 - c2;
}

int8_t strncmp_PP(const char *s1, const char *s2, uint16_t maxlen) {
  char c1, c2;

  do {
    c1 = pgm_read_byte(s1++);
    c2 = pgm_read_byte(s2++);
  } while (c1 && (c1 == c2) && maxlen--);
  return c1 - c2;
}

int sscanf_P(const char *str, const char *fmt, ...) {
  va_list vargs;
  int result;
  char _str[strlen_P(str) + 1];
  char _fmt[strlen_P(fmt) + 1];

  strcpy_P(_str, str);
  strcpy_P(_fmt, fmt);
  va_start(vargs, fmt);
  result = vsscanf(_str, _fmt, vargs);
  va_end(vargs);
  return result;
}
