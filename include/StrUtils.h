#pragma once

#include <WString.h>

String printfToString(const char *fmt, ...);

int8_t strcmp_PP(const char *s1, const char *s2);
int8_t strncmp_PP(const char *s1, const char *s2, uint16_t maxlen);

int sscanf_P(const char *str, const char *fmt, ...);
