#pragma once

#include <functional>
#include <Stream.h>
#ifdef ESP8266
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifdef ESP8266
static const char FALSE_PSTR[] PROGMEM = "false";
static const char TRUE_PSTR[] PROGMEM = "true";

static const char *const BOOLS[] PROGMEM = { FALSE_PSTR, TRUE_PSTR };
#else
static const char *const BOOLS[] = { "false", "true" };
#endif

enum cpevent_t : uint8_t { CP_INIT, CP_DONE, CP_RESTART, CP_WEB, CP_CONNECT, CP_DISCONNECT, CP_IDLE };

typedef std::function<void(cpevent_t event, void *param)> cpcallback_t;

struct __attribute__((__packed__)) editor_t {
  enum editortype_t : uint8_t { EDIT_NONE, EDIT_TEXT, EDIT_PASSWORD, EDIT_TEXTAREA, EDIT_CHECKBOX, EDIT_RADIO, EDIT_SELECT, EDIT_HIDDEN };

  editortype_t type;
  bool disabled : 1;
  bool required : 1;
  bool readonly : 1;
  union __attribute__((__packed__)) {
    struct __attribute__((__packed__)) {
      uint16_t size;
      uint16_t maxlength;
    } text;
    struct __attribute__((__packed__)) {
      uint16_t cols, rows;
      uint16_t maxlength;
    } textarea;
    struct __attribute__((__packed__)) {
      const char *checkedvalue;
      const char *uncheckedvalue;
    } checkbox;
    struct __attribute__((__packed__)) {
      uint16_t count;
      const char *const *values;
      const char *const *titles;
    } radio;
    struct __attribute__((__packed__)) {
      uint16_t size;
      uint16_t count;
      const char *const *values;
      const char *const *titles;
    } select;
  };
};

struct __attribute__((__packed__)) paraminfo_t {
  enum paramtype_t : uint8_t { PARAM_BOOL, PARAM_I8, PARAM_U8, PARAM_I16, PARAM_U16, PARAM_I32, PARAM_U32, PARAM_FLOAT, PARAM_CHAR, PARAM_STR, PARAM_BINARY, PARAM_IP };
  typedef uint8_t ipaddr_t[4];
  union __attribute__((__packed__)) paramvalue_t {
    bool asbool;
    int32_t asint;
    uint32_t asuint;
    float asfloat;
    char aschar;
    const char *asstr;
    const uint8_t *asbinary;
    const ipaddr_t asip;
  };
  union __attribute__((__packed__)) number_t {
    int32_t asint;
    uint32_t asuint;
    float asfloat;
  };

  const char *name;
  const char *title;
  paramtype_t type;
  uint16_t size;
  paramvalue_t defvalue;
  editor_t editor;
  number_t minvalue, maxvalue;
};

#define EDITOR_NONE() { .type = editor_t::EDIT_NONE }
#define EDITOR_TEXT(s, m, d, r, ro) { .type = editor_t::EDIT_TEXT, .disabled = (d), .required = (r), .readonly = (ro), { .text = { .size = (s), .maxlength = (m) } } }
#define EDITOR_PASSWORD(s, m, d, r, ro) { .type = editor_t::EDIT_PASSWORD, .disabled = (d), .required = (r), .readonly = (ro), { .text = { .size = (s), .maxlength = (m) } } }
#define EDITOR_TEXTAREA(c, r, m, d, re, ro) { .type = editor_t::EDIT_TEXTAREA, .disabled = (d), .required = (re), .readonly = (ro), { .textarea = { .cols = (c), .rows = (r), .maxlength = (m) } } }
#define EDITOR_CHECKBOX(c, u, d, r, ro) { .type = editor_t::EDIT_CHECKBOX, .disabled = (d), .required = (r), .readonly = (ro), { .checkbox = { .checkedvalue = (c), .uncheckedvalue = (u) } } }
#define EDITOR_RADIO(c, v, t, d, r, ro) { .type = editor_t::EDIT_RADIO, .disabled = (d), .required = (r), .readonly = (ro), { .radio = { .count = (c), .values = (v), .titles = (t) } } }
#define EDITOR_SELECT(s, c, v, t, d, r) { .type = editor_t::EDIT_SELECT, .disabled = (d), .required = (r), .readonly = false, { .select = { .size = (s), .count = (c), .values = (v), .titles = (t) } } }
#define EDITOR_HIDDEN() { .type = editor_t::EDIT_HIDDEN }

#define PARAM_BOOL_CUSTOM(n, t, d, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_BOOL, .size = sizeof(bool), .defvalue = { .asbool = (d) }, .editor = e }
#ifdef ESP8266
#define PARAM_BOOL(n, t, d) PARAM_BOOL_CUSTOM(n, t, d, EDITOR_CHECKBOX(TRUE_PSTR, FALSE_PSTR, false, false, false))
#else
#define PARAM_BOOL(n, t, d) PARAM_BOOL_CUSTOM(n, t, d, EDITOR_CHECKBOX(BOOLS[true], BOOLS[false], false, false, false))
#endif
#define PARAM_I8_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_I8, .size = sizeof(int8_t), .defvalue = { .asint = (d) }, .editor = e, .minvalue = { .asint = (m) }, .maxvalue = { .asint = (x) } }
#define PARAM_I8(n, t, d) PARAM_I8_CUSTOM(n, t, d, -128, 127, EDITOR_TEXT(3, 4, false, false, false))
#define PARAM_U8_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_U8, .size = sizeof(uint8_t), .defvalue = { .asuint = (d) }, .editor = e, .minvalue = { .asuint = (m) }, .maxvalue = { .asuint = (x) } }
#define PARAM_U8(n, t, d) PARAM_U8_CUSTOM(n, t, d, 0, 255, EDITOR_TEXT(3, 3, false, false, false))
#define PARAM_I16_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_I16, .size = sizeof(int16_t), .defvalue = { .asint = (d) }, .editor = e, .minvalue = { .asint = (m) }, .maxvalue = { .asint = (x) } }
#define PARAM_I16(n, t, d) PARAM_I16_CUSTOM(n, t, d, -32768, 32767, EDITOR_TEXT(5, 6, false, false, false))
#define PARAM_U16_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_U16, .size = sizeof(uint16_t), .defvalue = { .asuint = (d) }, .editor = e, .minvalue = { .asuint = (m) }, .maxvalue = { .asuint = (x) } }
#define PARAM_U16(n, t, d) PARAM_U16_CUSTOM(n, t, d, 0, 65535, EDITOR_TEXT(5, 5, false, false, false))
#define PARAM_I32_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_I32, .size = sizeof(int32_t), .defvalue = { .asint = (d) }, .editor = e, .minvalue = { .asint = (m) }, .maxvalue = { .asint = (x) } }
#define PARAM_I32(n, t, d) PARAM_I32_CUSTOM(n, t, d, -2147483648L, 2147483647L, EDITOR_TEXT(10, 11, false, false, false))
#define PARAM_U32_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_U32, .size = sizeof(uint32_t), .defvalue = { .asuint = (d) }, .editor = e, .minvalue = { .asuint = (m) }, .maxvalue = { .asuint = (x) } }
#define PARAM_U32(n, t, d) PARAM_U32_CUSTOM(n, t, d, 0, 4294967295UL, EDITOR_TEXT(10, 10, false, false, false))
#define PARAM_FLOAT_CUSTOM(n, t, d, m, x, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_FLOAT, .size = sizeof(float), .defvalue = { .asfloat = (d) }, .editor = e, .minvalue = { .asfloat = (m) }, .maxvalue = { .asfloat = (x) } }
#define PARAM_FLOAT(n, t, d) PARAM_FLOAT_CUSTOM(n, t, d, NAN, NAN, EDITOR_TEXT(10, 15, false, false, false))
#define PARAM_CHAR_CUSTOM(n, t, d, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_CHAR, .size = sizeof(char), .defvalue = { .aschar = (d) }, .editor = e }
#define PARAM_CHAR(n, t, d) PARAM_CHAR_CUSTOM(n, t, d, EDITOR_TEXT(1, 1, false, false, false))
#define PARAM_STR_CUSTOM(n, t, s, d, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_STR, .size = (s), .defvalue = { .asstr = (d) }, .editor = e }
#define PARAM_STR(n, t, s, d) PARAM_STR_CUSTOM(n, t, s, d, EDITOR_TEXT(((s) > 33 ? 32 : ((s) - 1)), ((s) - 1), false, false, false))
#define PARAM_PASSWORD(n, t, s, d) PARAM_STR_CUSTOM(n, t, s, d, EDITOR_PASSWORD(((s) > 33 ? 32 : ((s) - 1)), ((s) - 1), false, false, false))
#define PARAM_BINARY_CUSTOM(n, t, s, d, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_BINARY, .size = (s), .defvalue = { .asbinary = (d) }, .editor = e }
#define PARAM_BINARY(n, t, s, d) PARAM_BINARY_CUSTOM(n, t, s, d, EDITOR_TEXT((((s) + 2) / 3 * 4) > 32 ? 32 : (((s) + 2) / 3 * 4), (((s) + 2) / 3 * 4), false, false, false))
#define PARAM_IP_CUSTOM(n, t, d1, d2, d3, d4, e) { .name = (n), .title = (t), .type = paraminfo_t::PARAM_IP, .size = sizeof(paraminfo_t::ipaddr_t), .defvalue = { .asip = { (d1), (d2), (d3), (d4) }  }, .editor = e }
#define PARAM_IP(n, t, d1, d2, d3, d4) PARAM_IP_CUSTOM(n, t, d1, d2, d3, d4, EDITOR_TEXT(15, 15, false, false, false))

class Parameters {
public:
  Parameters(const paraminfo_t *params, uint16_t cnt) : _params(params), _count(cnt), _inited(false) {}

  bool begin();
  operator bool() {
    return _inited;
  }
  uint16_t count() const {
    return _count;
  }
  int16_t find(const char *name) const;
  const paraminfo_t *getInfo(uint16_t index) const;
  const char *name(uint16_t index) const;
  paraminfo_t::paramtype_t type(uint16_t index) const;
  uint16_t size(uint16_t index) const;
  uint16_t size(const char *name) const {
    return size(find(name));
  }
#ifdef ESP8266
  const void *value(uint16_t index) const;
#else
  const void *value(uint16_t index) const {
    return getPtr(index);
  }
#endif
  const void *value(const char *name) const {
    return value(find(name));
  }
  bool update();
  bool clear();
  void clear(uint16_t index);
  void clear(const char *name) {
    clear(find(name));
  }
  bool get(uint16_t index, void *data, uint16_t maxsize);
  bool get(const char *name, void *data, uint16_t maxsize) {
    return get(find(name), data, maxsize);
  }
  bool set(uint16_t index, const void *data);
  bool set(const char *name, const void *data) {
    return set(find(name), data);
  }
  String toString(uint16_t index, bool encode = false);
  String toString(const char *name, bool encode = false) {
    return toString(find(name), encode);
  }
  int16_t toStream(uint16_t index, Stream &stream, bool encode = false);
  int16_t toStream(const char *name, Stream &stream, bool encode = false) {
    return toStream(find(name), stream, encode);
  }
  bool fromString(uint16_t index, const String &str);
  bool fromString(const char *name, const String &str) {
    return fromString(find(name), str);
  }
  bool fromStream(uint16_t index, Stream &stream);
  bool fromStream(const char *name, Stream &stream) {
    return fromStream(find(name), stream);
  }

#ifdef ESP8266
  void handleWebPage(ESP8266WebServer &http, const char *restartPath = NULL, bool confirmation = true);
#else
  void handleWebPage(WebServer &http, const char *restartPath = NULL, bool confirmation = true);
#endif

protected:
  static const uint16_t EEPROM_SIGN = 0xA55A;

  struct __attribute__((__packed__)) header_t {
    uint16_t sign;
    uint16_t crc;
  };

  static uint16_t crc16(uint8_t data, uint16_t crc = 0xFFFF);
  static uint16_t crc16(const uint8_t *data, uint16_t size, uint16_t crc = 0xFFFF);

  void *getPtr(uint16_t index) const;
  bool check();

  String getScript();
  String getEditor(uint16_t index);

  String encodeString(char c);
  String encodeString(const char *str);

  const paraminfo_t *_params;
#ifdef ESP8266
  uint8_t _alignedData[4];
#endif
  uint16_t _count : 15;
  bool _inited : 1;
};

bool paramsCaptivePortal(Parameters *params, const char *ssid, const char *pswd, uint16_t duration = 0, cpcallback_t callback = NULL);
