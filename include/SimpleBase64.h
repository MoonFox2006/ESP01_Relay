#pragma once

#include <Stream.h>

String encodeBase64(const uint8_t *data, uint16_t size);
int16_t encodeBase64(Stream &stream, const uint8_t *data, uint16_t size);
int16_t decodeBase64(const char *str, uint8_t *data, uint16_t maxsize);
int16_t decodeBase64(Stream &stream, uint8_t *data, uint16_t maxsize);
