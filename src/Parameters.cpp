#include <stdio.h>
#include <string.h>
#ifdef ESP32
#include <esp_log.h>
#endif
#include <EEPROM.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <DNSServer.h>
#include <StreamString.h>
#include "Parameters.h"
#ifdef ESP8266
#include "StrUtils.h"
#endif
#include "Base64.h"

#ifdef ESP32
static const char TAG[] = "Parameters";
#endif

#ifdef ESP8266
static const char EMPTY_PSTR[] PROGMEM = "";

static const char FMT_I8_PSTR[] PROGMEM = "%hhd";
static const char FMT_U8_PSTR[] PROGMEM = "%hhu";
static const char FMT_I16_PSTR[] PROGMEM = "%hd";
static const char FMT_U16_PSTR[] PROGMEM = "%hu";
static const char FMT_I32_PSTR[] PROGMEM = "%d";
static const char FMT_U32_PSTR[] PROGMEM = "%u";
static const char FMT_FLOAT_PSTR[] PROGMEM = "%f";
static const char FMT_IP_PSTR[] PROGMEM = "%hhu.%hhu.%hhu.%hhu";

static const char QUOT_PSTR[] PROGMEM = "&quot;";
static const char LT_PSTR[] PROGMEM = "&lt;";
static const char GT_PSTR[] PROGMEM = "&gt;";

static const char TEXTHTML_PSTR[] PROGMEM = "text/html";
static const char TEXTPLAIN_PSTR[] PROGMEM = "text/plain";
#else
static const char EMPTY_PSTR[] = "";

static const char FMT_I8_PSTR[] = "%hhd";
static const char FMT_U8_PSTR[] = "%hhu";
static const char FMT_I16_PSTR[] = "%hd";
static const char FMT_U16_PSTR[] = "%hu";
static const char FMT_I32_PSTR[] = "%d";
static const char FMT_U32_PSTR[] = "%u";
static const char FMT_FLOAT_PSTR[] = "%f";
static const char FMT_IP_PSTR[] = "%hhu.%hhu.%hhu.%hhu";

static const char QUOT_PSTR[] = "&quot;";
static const char LT_PSTR[] = "&lt;";
static const char GT_PSTR[] = "&gt;";

static const char TEXTHTML_PSTR[] = "text/html";
static const char TEXTPLAIN_PSTR[] = "text/plain";
#endif

bool Parameters::begin() {
  if (! _inited) {
    uint16_t size = sizeof(header_t);

    for (uint16_t i = 0; i < _count; ++i) {
#ifdef ESP8266
      size += pgm_read_word(&_params[i].size);
#else
      size += _params[i].size;
#endif
    }
#ifdef ESP8266
    EEPROM.begin(size);
    _inited = true;
#else
    _inited = EEPROM.begin(size);
    if (! _inited) {
      ESP_LOGE(TAG, "Error allocating of EEPROM (%hu bytes)!", size);
      return false;
    }
#endif
    if (! check()) {
      clear();
#ifdef ESP32
      ESP_LOGW(TAG, "Reset EEPROM parameters!");
#endif
    }
  }
  return _inited;
}

int16_t Parameters::find(const char *name) const {
  for (uint16_t i = 0; i < _count; ++i) {
#ifdef ESP8266
    if (! strcmp_PP((char*)pgm_read_ptr(&_params[i].name), name))
#else
    if (! strcmp(_params[i].name, name))
#endif
      return i;
  }
  return -1;
}

const paraminfo_t *Parameters::getInfo(uint16_t index) const {
  if (index < _count)
    return &_params[index];
  return NULL;
}

const char *Parameters::name(uint16_t index) const {
  if (index < _count)
#ifdef ESP8266
    return (char*)pgm_read_ptr(&_params[index].name);
#else
    return _params[index].name;
#endif
  return NULL;
}

paraminfo_t::paramtype_t Parameters::type(uint16_t index) const {
  if (index < _count)
#ifdef ESP8266
    return (paraminfo_t::paramtype_t)pgm_read_byte(&_params[index].type);
#else
    return _params[index].type;
#endif
}

uint16_t Parameters::size(uint16_t index) const {
  if (index < _count)
#ifdef ESP8266
    return pgm_read_word(&_params[index].size);
#else
    return _params[index].size;
#endif
  return 0;
}

#ifdef ESP8266
const void *Parameters::value(uint16_t index) const {
  const void *result = NULL;

  if (_inited && (index < _count)) {
    result = getPtr(index);

    if (result) {
      paraminfo_t::paramtype_t type = (paraminfo_t::paramtype_t)pgm_read_byte(&_params[index].type);
      uint16_t size = pgm_read_word(&_params[index].size);

      if ((size > 1) && ((type != paraminfo_t::PARAM_STR) && (type != paraminfo_t::PARAM_BINARY))) {
        memcpy((void*)_alignedData, result, size);
        result = _alignedData;
      }
    }
  }
  return result;
}
#endif

bool Parameters::clear() {
  for (uint16_t i = 0; i < _count; ++i) {
    clear(i);
  }
  return update();
}

void Parameters::clear(uint16_t index) {
  if (_inited && (index < _count)) {
#ifdef ESP8266
    switch (pgm_read_byte(&_params[index].type)) {
#else
    switch (_params[index].type) {
#endif
      case paraminfo_t::PARAM_BOOL:
      case paraminfo_t::PARAM_U8:
      case paraminfo_t::PARAM_U16:
      case paraminfo_t::PARAM_I32:
      case paraminfo_t::PARAM_U32:
      case paraminfo_t::PARAM_FLOAT:
      case paraminfo_t::PARAM_CHAR:
        set(index, &_params[index].defvalue);
        break;
      case paraminfo_t::PARAM_I8:
        {
#ifdef ESP8266
          int8_t i8 = (int32_t)pgm_read_dword(&_params[index].defvalue.asint);
#else
          int8_t i8 = _params[index].defvalue.asint;
#endif

          set(index, &i8);
        }
        break;
      case paraminfo_t::PARAM_I16:
        {
#ifdef ESP8266
          int16_t i16 = (int32_t)pgm_read_dword(&_params[index].defvalue.asint);
#else
          int16_t i16 = _params[index].defvalue.asint;
#endif

          set(index, &i16);
        }
        break;
      case paraminfo_t::PARAM_STR:
#ifdef ESP8266
        set(index, pgm_read_ptr(&_params[index].defvalue.asstr));
#else
        set(index, _params[index].defvalue.asstr);
#endif
        break;
      case paraminfo_t::PARAM_BINARY:
#ifdef ESP8266
        set(index, pgm_read_ptr(&_params[index].defvalue.asbinary));
#else
        set(index, _params[index].defvalue.asbinary);
#endif
        break;
      case paraminfo_t::PARAM_IP:
#ifdef ESP8266
        set(index, &_params[index].defvalue.asip[0]);
#else
        set(index, _params[index].defvalue.asip);
#endif
        break;
    }
  }
}

bool Parameters::get(uint16_t index, void *data, uint16_t maxsize) {
  if (_inited && (index < _count) && data && maxsize) {
    const void *ptr = getPtr(index);

    if (ptr) {
#ifdef ESP8266
      paraminfo_t::paramtype_t type = (paraminfo_t::paramtype_t)pgm_read_byte(&_params[index].type);
      uint16_t size = pgm_read_word(&_params[index].size);

      if ((type < paraminfo_t::PARAM_STR) || (type == paraminfo_t::PARAM_IP)) {
        if (maxsize >= size) {
          memcpy(data, ptr, size);
#else
      if ((_params[index].type < paraminfo_t::PARAM_STR) || (_params[index].type == paraminfo_t::PARAM_IP)) {
        if (maxsize >= _params[index].size) {
          memcpy(data, ptr, _params[index].size);
#endif
          return true;
        }
      } else { // type == PARAM_STR or PARAM_BINARY
#ifdef ESP8266
        if (maxsize > size)
          maxsize = size;
        if (type == paraminfo_t::PARAM_STR) {
#else
        if (maxsize > _params[index].size)
          maxsize = _params[index].size;
        if (_params[index].type == paraminfo_t::PARAM_STR) {
#endif
          memcpy(data, ptr, maxsize - 1);
          ((char*)data)[maxsize - 1] = '\0';
        } else { // type == PARAM_BINARY
          memcpy(data, ptr, maxsize);
        }
        return true;
      }
    }
  }
  return false;
}

bool Parameters::set(uint16_t index, const void *data) {
  if (_inited && (index < _count)) {
    void *ptr = getPtr(index);

    if (ptr) {
#ifdef ESP8266
      uint16_t size = pgm_read_word(&_params[index].size);

      memset(ptr, 0, size);
#else
      memset(ptr, 0, _params[index].size);
#endif
      if (data) {
#ifdef ESP8266
        if (pgm_read_byte(&_params[index].type) != paraminfo_t::PARAM_STR) {
          memcpy_P(ptr, data, size);
        } else { // type == PARAM_STR
          strncpy_P((char*)ptr, (char*)data, size - 1);
//          ((char*)ptr)[size - 1] = '\0';
#else
        if (_params[index].type != paraminfo_t::PARAM_STR) {
          memcpy(ptr, data, _params[index].size);
        } else { // type == PARAM_STR
          strncpy((char*)ptr, (char*)data, _params[index].size - 1);
//          ((char*)ptr)[_params[index].size - 1] = '\0';
#endif
        }
      }
      return true;
    }
  }
  return false;
}

String Parameters::toString(uint16_t index, bool encode) {
  if (_inited && (index < _count)) {
    StreamString result;

    if (toStream(index, result, encode) >= 0)
      return result;
  }
  return String();
}

#ifdef ESP8266
int16_t Parameters::toStream(uint16_t index, Stream &stream, bool encode) {
  int16_t result = -1;

  if (_inited && (index < _count)) {
    const void *ptr = value(index);

    if (ptr) {
      switch (pgm_read_byte(&_params[index].type)) {
        case paraminfo_t::PARAM_BOOL:
          if (*(bool*)ptr)
            result = stream.print(FPSTR((char*)pgm_read_ptr(&BOOLS[true])));
          else
            result = stream.print(FPSTR((char*)pgm_read_ptr(&BOOLS[false])));
          break;
        case paraminfo_t::PARAM_I8:
          result = stream.printf_P(FMT_I8_PSTR, *(int8_t*)ptr);
          break;
        case paraminfo_t::PARAM_U8:
          result = stream.printf_P(FMT_U8_PSTR, *(uint8_t*)ptr);
          break;
        case paraminfo_t::PARAM_I16:
          result = stream.printf_P(FMT_I16_PSTR, *(int16_t*)ptr);
          break;
        case paraminfo_t::PARAM_U16:
          result = stream.printf_P(FMT_U16_PSTR, *(uint16_t*)ptr);
          break;
        case paraminfo_t::PARAM_I32:
          result = stream.printf_P(FMT_I32_PSTR, *(int32_t*)ptr);
          break;
        case paraminfo_t::PARAM_U32:
          result = stream.printf_P(FMT_U32_PSTR, *(uint32_t*)ptr);
          break;
        case paraminfo_t::PARAM_FLOAT:
          result = stream.printf_P(FMT_FLOAT_PSTR, *(float*)ptr);
          break;
        case paraminfo_t::PARAM_CHAR:
          if (encode)
            result = stream.print(encodeString(*(char*)ptr));
          else
            result = stream.print(*(char*)ptr);
          break;
        case paraminfo_t::PARAM_STR:
          if (encode)
            result = stream.print(encodeString((char*)ptr));
          else
            result = stream.print((char*)ptr);
          break;
        case paraminfo_t::PARAM_BINARY:
          result = encodeBase64(stream, (uint8_t*)ptr, pgm_read_word(&_params[index].size));
          break;
        case paraminfo_t::PARAM_IP:
          result = stream.printf_P(FMT_IP_PSTR, (*(paraminfo_t::ipaddr_t*)ptr)[0], (*(paraminfo_t::ipaddr_t*)ptr)[1],
            (*(paraminfo_t::ipaddr_t*)ptr)[2], (*(paraminfo_t::ipaddr_t*)ptr)[3]);
          break;
      }
    }
  }
  return result;
}

#else
int16_t Parameters::toStream(uint16_t index, Stream &stream, bool encode) {
  int16_t result = -1;

  if (_inited && (index < _count)) {
    const void *ptr = getPtr(index);

    if (ptr) {
      switch (_params[index].type) {
        case paraminfo_t::PARAM_BOOL:
          if (*(bool*)ptr)
            result = stream.print(BOOLS[true]);
          else
            result = stream.print(BOOLS[false]);
          break;
        case paraminfo_t::PARAM_I8:
          result = stream.printf(FMT_I8_PSTR, *(int8_t*)ptr);
          break;
        case paraminfo_t::PARAM_U8:
          result = stream.printf(FMT_U8_PSTR, *(uint8_t*)ptr);
          break;
        case paraminfo_t::PARAM_I16:
          result = stream.printf(FMT_I16_PSTR, *(int16_t*)ptr);
          break;
        case paraminfo_t::PARAM_U16:
          result = stream.printf(FMT_U16_PSTR, *(uint16_t*)ptr);
          break;
        case paraminfo_t::PARAM_I32:
          result = stream.printf(FMT_I32_PSTR, *(int32_t*)ptr);
          break;
        case paraminfo_t::PARAM_U32:
          result = stream.printf(FMT_U32_PSTR, *(uint32_t*)ptr);
          break;
        case paraminfo_t::PARAM_FLOAT:
          result = stream.printf(FMT_FLOAT_PSTR, *(float*)ptr);
          break;
        case paraminfo_t::PARAM_CHAR:
          if (encode)
            result = stream.print(encodeString(*(char*)ptr));
          else
            result = stream.print(*(char*)ptr);
          break;
        case paraminfo_t::PARAM_STR:
          if (encode)
            result = stream.print(encodeString((char*)ptr));
          else
            result = stream.print((char*)ptr);
          break;
        case paraminfo_t::PARAM_BINARY:
          result = encodeBase64(stream, (uint8_t*)ptr, _params[index].size);
          break;
        case paraminfo_t::PARAM_IP:
          result = stream.printf(FMT_IP_PSTR, (*(paraminfo_t::ipaddr_t*)ptr)[0], (*(paraminfo_t::ipaddr_t*)ptr)[1],
            (*(paraminfo_t::ipaddr_t*)ptr)[2], (*(paraminfo_t::ipaddr_t*)ptr)[3]);
          break;
      }
    }
  }
  return result;
}
#endif

#ifdef ESP8266
bool Parameters::fromString(uint16_t index, const String &str) {
  bool result = false;

  if (_inited && (index < _count)) {
    void *ptr = getPtr(index);

    if (ptr) {
      uint8_t data[4];

      switch (pgm_read_byte(&_params[index].type)) {
        case paraminfo_t::PARAM_BOOL:
          if (str.equals(FPSTR((char*)pgm_read_ptr(&BOOLS[true]))) || str.equals(F("1"))) {
            *(bool*)ptr = true;
            result = true;
          } else if (str.equals(FPSTR((char*)pgm_read_ptr(&BOOLS[false]))) || str.equals(F("0"))) {
            *(bool*)ptr = false;
            result = true;
          }
          break;
        case paraminfo_t::PARAM_I8:
          result = sscanf_P(str.c_str(), FMT_I8_PSTR, (int8_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_U8:
          result = sscanf_P(str.c_str(), FMT_U8_PSTR, (uint8_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_I16:
          result = sscanf_P(str.c_str(), FMT_I16_PSTR, (int16_t*)data) == 1;
          if (result)
            set(index, data);
          break;
        case paraminfo_t::PARAM_U16:
          result = sscanf_P(str.c_str(), FMT_U16_PSTR, (uint16_t*)data) == 1;
          if (result)
            set(index, data);
          break;
        case paraminfo_t::PARAM_I32:
          result = sscanf_P(str.c_str(), FMT_I32_PSTR, (int32_t*)data) == 1;
          if (result)
            set(index, data);
          break;
        case paraminfo_t::PARAM_U32:
          result = sscanf_P(str.c_str(), FMT_U32_PSTR, (uint32_t*)data) == 1;
          if (result)
            set(index, data);
          break;
        case paraminfo_t::PARAM_FLOAT:
          result = sscanf_P(str.c_str(), FMT_FLOAT_PSTR, (float*)data) == 1;
          if (result)
            set(index, data);
          break;
        case paraminfo_t::PARAM_CHAR:
          *(char*)ptr = str[0];
          result = true;
          break;
        case paraminfo_t::PARAM_STR:
          strncpy((char*)ptr, str.c_str(), pgm_read_word(&_params[index].size) - 1);
          ((char*)ptr)[pgm_read_word(&_params[index].size) - 1] = '\0';
          result = true;
          break;
        case paraminfo_t::PARAM_BINARY:
          result = decodeBase64(str.c_str(), (uint8_t*)ptr, pgm_read_word(&_params[index].size)) != -1;
          break;
        case paraminfo_t::PARAM_IP:
          result = sscanf_P(str.c_str(), FMT_IP_PSTR, &data[0], &data[1], &data[2], &data[3]);
          if (result)
            set(index, data);
          break;
      }
      if (! result) {
        clear(index);
      }
    }
  }
  return result;
}

#else
bool Parameters::fromString(uint16_t index, const String &str) {
  bool result = false;

  if (_inited && (index < _count)) {
    void *ptr = getPtr(index);

    if (ptr) {
      switch (_params[index].type) {
        case paraminfo_t::PARAM_BOOL:
          if (str.equals(BOOLS[true]) || str.equals("1")) {
            *(bool*)ptr = true;
            result = true;
          } else if (str.equals(BOOLS[false]) || str.equals("0")) {
            *(bool*)ptr = false;
            result = true;
          }
          break;
        case paraminfo_t::PARAM_I8:
          result = sscanf(str.c_str(), FMT_I8_PSTR, (int8_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_U8:
          result = sscanf(str.c_str(), FMT_U8_PSTR, (uint8_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_I16:
          result = sscanf(str.c_str(), FMT_I16_PSTR, (int16_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_U16:
          result = sscanf(str.c_str(), FMT_U16_PSTR, (uint16_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_I32:
          result = sscanf(str.c_str(), FMT_I32_PSTR, (int32_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_U32:
          result = sscanf(str.c_str(), FMT_U32_PSTR, (uint32_t*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_FLOAT:
          result = sscanf(str.c_str(), FMT_FLOAT_PSTR, (float*)ptr) == 1;
          break;
        case paraminfo_t::PARAM_CHAR:
          *(char*)ptr = str[0];
          result = true;
          break;
        case paraminfo_t::PARAM_STR:
          strncpy((char*)ptr, str.c_str(), _params[index].size - 1);
          ((char*)ptr)[_params[index].size - 1] = '\0';
          result = true;
          break;
        case paraminfo_t::PARAM_BINARY:
          result = decodeBase64(str.c_str(), (uint8_t*)ptr, _params[index].size) != -1;
          break;
        case paraminfo_t::PARAM_IP:
          result = sscanf(str.c_str(), FMT_IP_PSTR, &(*(paraminfo_t::ipaddr_t*)ptr)[0], &(*(paraminfo_t::ipaddr_t*)ptr)[1],
            &(*(paraminfo_t::ipaddr_t*)ptr)[2], &(*(paraminfo_t::ipaddr_t*)ptr)[3]) == 4;
          break;
      }
      if (! result) {
        clear(index);
      }
    }
  }
  return result;
}
#endif

bool Parameters::fromStream(uint16_t index, Stream &stream) {
  if (_inited && (index < _count)) {
    return fromString(index, stream.readString());
  }
  return false;
}

#ifdef ESP8266
String Parameters::getScript() {
  return String(F("<script type=\"text/javascript\">\n"
    "function getXmlHttpRequest(){\n"
    "let x;\n"
    "try{\n"
    "x=new ActiveXObject(\"Msxml2.XMLHTTP\");\n"
    "}catch(e){\n"
    "try{\n"
    "x=newActiveXObject(\"Microsoft.XMLHTTP\");\n"
    "}catch(E){\n"
    "x=false;\n"
    "}\n"
    "}\n"
    "if((!x)&&(typeof XMLHttpRequest!='undefined')){\n"
    "x=new XMLHttpRequest();\n"
    "}\n"
    "return x;\n"
    "}\n"
    "function openUrl(u,m){\n"
    "let x=getXmlHttpRequest();\n"
    "x.open(m,u,false);\n"
    "x.send(null);\n"
    "if(x.status!=200){\n"
    "alert(x.responseText);\n"
    "return false;\n"
    "}\n"
    "return true;\n"
    "}\n"
    "function checkInt(e,d,m,x){\n"
    "let n=parseInt(e.value);\n"
    "if(isNaN(n)){\n"
    "n=d;\n"
    "}else{\n"
    "if(n<m)n=m;\n"
    "if(n>x)n=x;\n"
    "}\n"
    "e.value=n.toString();\n"
    "}\n"
    "function checkFloat(e,d,m,x){\n"
    "let n=parseFloat(e.value);\n"
    "if(isNaN(n)){\n"
    "n=d;\n"
    "}else{\n"
    "if((!isNaN(m))&&(n<m))n=m;\n"
    "if((!isNaN(x))&&(n>x))n=x;\n"
    "}\n"
    "if(isNaN(n)){e.value=\"\";\n"
    "}else{\n"
    "e.value=n.toString();\n"
    "}\n"
    "}\n"
    "function processTab(t,e){\n"
    "if(e.which==9){\n"
    "let start=t.selectionStart;\n"
    "let end=t.selectionEnd;\n"
    "t.value=t.value.substr(0,start)+'\t'+t.value.substr(end);\n"
    "t.selectionStart=t.selectionEnd=start+1;\n"
    "e.preventDefault();\n"
    "return false;\n"
    "}\n"
    "}\n"
    "</script>\n"));
}
#else
String Parameters::getScript() {
  return String("<script type=\"text/javascript\">\n"
    "function getXmlHttpRequest(){\n"
    "let x;\n"
    "try{\n"
    "x=new ActiveXObject(\"Msxml2.XMLHTTP\");\n"
    "}catch(e){\n"
    "try{\n"
    "x=newActiveXObject(\"Microsoft.XMLHTTP\");\n"
    "}catch(E){\n"
    "x=false;\n"
    "}\n"
    "}\n"
    "if((!x)&&(typeof XMLHttpRequest!='undefined')){\n"
    "x=new XMLHttpRequest();\n"
    "}\n"
    "return x;\n"
    "}\n"
    "function openUrl(u,m){\n"
    "let x=getXmlHttpRequest();\n"
    "x.open(m,u,false);\n"
    "x.send(null);\n"
    "if(x.status!=200){\n"
    "alert(x.responseText);\n"
    "return false;\n"
    "}\n"
    "return true;\n"
    "}\n"
    "function checkInt(e,d,m,x){\n"
    "let n=parseInt(e.value);\n"
    "if(isNaN(n)){\n"
    "n=d;\n"
    "}else{\n"
    "if(n<m)n=m;\n"
    "if(n>x)n=x;\n"
    "}\n"
    "e.value=n.toString();\n"
    "}\n"
    "function checkFloat(e,d,m,x){\n"
    "let n=parseFloat(e.value);\n"
    "if(isNaN(n)){\n"
    "n=d;\n"
    "}else{\n"
    "if((!isNaN(m))&&(n<m))n=m;\n"
    "if((!isNaN(x))&&(n>x))n=x;\n"
    "}\n"
    "if(isNaN(n)){e.value=\"\";\n"
    "}else{\n"
    "e.value=n.toString();\n"
    "}\n"
    "}\n"
    "function processTab(t,e){\n"
    "if(e.which==9){\n"
    "let start=t.selectionStart;\n"
    "let end=t.selectionEnd;\n"
    "t.value=t.value.substr(0,start)+'\t'+t.value.substr(end);\n"
    "t.selectionStart=t.selectionEnd=start+1;\n"
    "e.preventDefault();\n"
    "return false;\n"
    "}\n"
    "}\n"
    "</script>\n");
}
#endif

#ifdef ESP8266
String Parameters::getEditor(uint16_t index) {
  static const char DISABLED_PSTR[] PROGMEM = " disabled";
  static const char REQUIRED_PSTR[] PROGMEM = " required";
  static const char READONLY_PSTR[] PROGMEM = " readonly";
  static const char CHECKED_PSTR[] PROGMEM = " checked";
  static const char SELECTED_PSTR[] PROGMEM = " selected";
  static const char SIZE_PSTR[] PROGMEM = " size=";
  static const char MAXLENGTH_PSTR[] PROGMEM = " maxlength=";
  static const char NAN_PSTR[] PROGMEM = "NaN";

  if (_inited && (index < _count) && (pgm_read_byte(&_params[index].editor.type) != editor_t::EDIT_NONE) && getPtr(index)) {
    StreamString result;
    editor_t editor;

    memcpy_P(&editor, &_params[index].editor, sizeof(editor_t));
    if (editor.type == editor_t::EDIT_SELECT) {
      result.print(F("<select name=\""));
      result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
      result.print('"');
      if (editor.select.size) {
        result.print(FPSTR(SIZE_PSTR));
        result.print(editor.select.size);
      }
      if (editor.disabled) {
        result.print(FPSTR(DISABLED_PSTR));
      }
      if (editor.required) {
        result.print(FPSTR(REQUIRED_PSTR));
      }
      result.print(F(">\n"));
      if (editor.select.count && editor.select.values) {
        for (uint16_t i = 0; i < editor.select.count; ++i) {
          result.print(F("<option value=\""));
          result.print(encodeString((char*)pgm_read_ptr(&editor.select.values[i])));
          result.print('"');
          if (toString(index).equals(FPSTR((char*)pgm_read_ptr(&editor.select.values[i])))) {
            result.print(FPSTR(SELECTED_PSTR));
          }
          result.print('>');
          if (editor.select.titles)
            result.print(encodeString((char*)pgm_read_ptr(&editor.select.titles[i])));
          else
            result.print(encodeString((char*)pgm_read_ptr(&editor.select.values[i])));
          result.print(F("</option>\n"));
        }
      }
      result.print(F("</select>"));
    } else if (editor.type == editor_t::EDIT_RADIO) {
      if (editor.radio.count && editor.radio.values) {
        for (uint16_t i = 0; i < editor.radio.count; ++i) {
          result.print(F("<input type=\"radio\" name=\""));
          result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
          result.print(F("\" value=\""));
          result.print(encodeString((char*)pgm_read_ptr(&editor.radio.values[i])));
          result.print('"');
          if (toString(index).equals(FPSTR((char*)pgm_read_ptr(&editor.radio.values[i])))) {
            result.print(FPSTR(CHECKED_PSTR));
          }
          if (editor.disabled) {
            result.print(FPSTR(DISABLED_PSTR));
          }
          if (editor.required) {
            result.print(FPSTR(REQUIRED_PSTR));
          }
          if (editor.readonly) {
            result.print(F(" onclick=\"return false;\""));
          }
          result.print('>');
          if (editor.radio.titles)
            result.print(encodeString((char*)pgm_read_ptr(&editor.radio.titles[i])));
          else
            result.print(encodeString((char*)pgm_read_ptr(&editor.radio.values[i])));
          result.print('\n');
        }
      }
    } else if (editor.type == editor_t::EDIT_TEXTAREA) {
      result.print(F("<textarea name=\""));
      result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
      result.print('"');
      if (editor.textarea.cols) {
        result.print(F(" cols="));
        result.print(editor.textarea.cols);
      }
      if (editor.textarea.rows) {
        result.print(F(" rows="));
        result.print(editor.textarea.rows);
      }
      if (editor.textarea.maxlength) {
        result.print(FPSTR(MAXLENGTH_PSTR));
        result.print(editor.textarea.maxlength);
      }
      if (editor.disabled) {
        result.print(FPSTR(DISABLED_PSTR));
      }
      if (editor.required) {
        result.print(FPSTR(REQUIRED_PSTR));
      }
      if (editor.readonly) {
        result.print(FPSTR(READONLY_PSTR));
      }
      result.print(F(" onkeydown=\"processTab(this,event);\">"));
      toStream(index, result, true);
      result.print(F("</textarea>"));
    } else {
      result.print(F("<input type=\""));
      if (editor.type == editor_t::EDIT_TEXT)
        result.print(F("text"));
      else if (editor.type == editor_t::EDIT_PASSWORD)
        result.print(F("password"));
      else if (editor.type == editor_t::EDIT_CHECKBOX)
        result.print(F("checkbox"));
      else if (editor.type == editor_t::EDIT_HIDDEN)
        result.print(F("hidden"));
      result.print(F("\" name=\""));
      result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
      result.print('"');
      if (editor.type != editor_t::EDIT_HIDDEN) {
        if (editor.disabled) {
          result.print(FPSTR(DISABLED_PSTR));
        }
        if (editor.required) {
          result.print(FPSTR(REQUIRED_PSTR));
        }
      }
      if ((editor.type == editor_t::EDIT_TEXT) || (editor.type == editor_t::EDIT_PASSWORD)) {
        if (editor.text.size) {
          result.print(FPSTR(SIZE_PSTR));
          result.print(editor.text.size);
        }
        if (editor.text.maxlength) {
          result.print(FPSTR(MAXLENGTH_PSTR));
          result.print(editor.text.maxlength);
        }
        if (editor.readonly) {
          result.print(FPSTR(READONLY_PSTR));
        }
      } else if (editor.type == editor_t::EDIT_CHECKBOX) {
        result.print(F(" value=\""));
        result.print(encodeString((char*)pgm_read_ptr(&editor.checkbox.checkedvalue)));
        result.print('"');
        if (toString(index).equals(FPSTR((char*)pgm_read_ptr(&editor.checkbox.checkedvalue)))) {
          result.print(FPSTR(CHECKED_PSTR));
        }
        if (editor.readonly) {
          result.print(F(" onclick=\"return false;\""));
        } else {
          result.print(F(" onchange=\"document.getElementsByName('"));
          result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
          result.print(F("')[1].disabled=this.checked;\""));
        }
        result.print(F("><input type=\"hidden\" name=\""));
        result.print(FPSTR((char*)pgm_read_ptr(&_params[index].name)));
        result.print(F("\" value=\""));
        result.print(encodeString((char*)pgm_read_ptr(&editor.checkbox.uncheckedvalue)));
        result.print('"');
        if (toString(index).equals(FPSTR((char*)pgm_read_ptr(&editor.checkbox.checkedvalue)))) {
          result.print(FPSTR(DISABLED_PSTR));
        }
        result.print('>');
      }
      if ((editor.type == editor_t::EDIT_TEXT) || (editor.type == editor_t::EDIT_PASSWORD) ||
        (editor.type == editor_t::EDIT_HIDDEN)) {
        result.print(F(" value=\""));
        toStream(index, result, true);
        result.print('"');
        if (editor.type == editor_t::EDIT_TEXT) {
          if (! editor.readonly) {
            paraminfo_t::paramtype_t type = (paraminfo_t::paramtype_t)pgm_read_byte(&_params[index].type);

            if ((type >= paraminfo_t::PARAM_I8) && (type <= paraminfo_t::PARAM_U32)) { // Integers
              result.print(F(" onblur=\"checkInt(this,"));
              if ((type == paraminfo_t::PARAM_I8) || (type == paraminfo_t::PARAM_I16) ||
                (type == paraminfo_t::PARAM_I32)) {
                result.printf_P(FMT_I32_PSTR, (int32_t)pgm_read_dword(&_params[index].defvalue.asint));
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf_P(FMT_U32_PSTR, pgm_read_dword(&_params[index].defvalue.asuint));
              }
              result.print(',');
              if ((type == paraminfo_t::PARAM_I8) || (type == paraminfo_t::PARAM_I16) ||
                (type == paraminfo_t::PARAM_I32)) {
                result.printf_P(FMT_I32_PSTR, (int32_t)pgm_read_dword(&_params[index].minvalue.asint));
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf_P(FMT_U32_PSTR, pgm_read_dword(&_params[index].minvalue.asuint));
              }
              result.print(',');
              if ((type == paraminfo_t::PARAM_I8) || (type == paraminfo_t::PARAM_I16) ||
                (type == paraminfo_t::PARAM_I32)) {
                result.printf_P(FMT_I32_PSTR, (int32_t)pgm_read_dword(&_params[index].maxvalue.asint));
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf_P(FMT_U32_PSTR, pgm_read_dword(&_params[index].maxvalue.asuint));
              }
              result.print(F(");\""));
            } else if (type == paraminfo_t::PARAM_FLOAT) {
              result.print(F(" onblur=\"checkFloat(this,"));
              if (isnan(pgm_read_float(&_params[index].defvalue.asfloat))) {
                result.print(FPSTR(NAN_PSTR));
              } else {
                result.printf_P(FMT_FLOAT_PSTR, pgm_read_float(&_params[index].defvalue.asfloat));
              }
              result.print(',');
              if (isnan(pgm_read_float(&_params[index].minvalue.asfloat))) {
                result.print(FPSTR(NAN_PSTR));
              } else {
                result.printf_P(FMT_FLOAT_PSTR, pgm_read_float(&_params[index].minvalue.asfloat));
              }
              result.print(',');
              if (isnan(pgm_read_float(&_params[index].maxvalue.asfloat))) {
                result.print(FPSTR(NAN_PSTR));
              } else {
                result.printf_P(FMT_FLOAT_PSTR, pgm_read_float(&_params[index].maxvalue.asfloat));
              }
              result.print(F(");\""));
            }
          }
        }
        result.print('>');
      }
    }
    return result;
  }
  return String();
}

#else
String Parameters::getEditor(uint16_t index) {
  static const char DISABLED_PSTR[] = " disabled";
  static const char REQUIRED_PSTR[] = " required";
  static const char READONLY_PSTR[] = " readonly";
  static const char CHECKED_PSTR[] = " checked";
  static const char SELECTED_PSTR[] = " selected";
  static const char SIZE_PSTR[] = " size=";
  static const char MAXLENGTH_PSTR[] = " maxlength=";
  static const char NAN_PSTR[] = "NaN";

  if (_inited && (index < _count) && (_params[index].editor.type != editor_t::EDIT_NONE) && getPtr(index)) {
    StreamString result;

    if (_params[index].editor.type == editor_t::EDIT_SELECT) {
      result.print("<select name=\"");
      result.print(_params[index].name);
      result.print('"');
      if (_params[index].editor.select.size) {
        result.print(SIZE_PSTR);
        result.print(_params[index].editor.select.size);
      }
      if (_params[index].editor.disabled) {
        result.print(DISABLED_PSTR);
      }
      if (_params[index].editor.required) {
        result.print(REQUIRED_PSTR);
      }
      result.print(">\n");
      if (_params[index].editor.select.count && _params[index].editor.select.values) {
        for (uint16_t i = 0; i < _params[index].editor.select.count; ++i) {
          result.print("<option value=\"");
          result.print(encodeString(_params[index].editor.select.values[i]));
          result.print('"');
          if (toString(index).equals(_params[index].editor.select.values[i])) {
            result.print(SELECTED_PSTR);
          }
          result.print('>');
          if (_params[index].editor.select.titles)
            result.print(encodeString(_params[index].editor.select.titles[i]));
          else
            result.print(encodeString(_params[index].editor.select.values[i]));
          result.print("</option>\n");
        }
      }
      result.print("</select>");
    } else if (_params[index].editor.type == editor_t::EDIT_RADIO) {
      if (_params[index].editor.radio.count && _params[index].editor.radio.values) {
        for (uint16_t i = 0; i < _params[index].editor.radio.count; ++i) {
          result.print("<input type=\"radio\" name=\"");
          result.print(_params[index].name);
          result.print("\" value=\"");
          result.print(encodeString(_params[index].editor.radio.values[i]));
          result.print('"');
          if (toString(index).equals(_params[index].editor.radio.values[i])) {
            result.print(CHECKED_PSTR);
          }
          if (_params[index].editor.disabled) {
            result.print(DISABLED_PSTR);
          }
          if (_params[index].editor.required) {
            result.print(REQUIRED_PSTR);
          }
          if (_params[index].editor.readonly) {
            result.print(" onclick=\"return false;\"");
          }
          result.print('>');
          if (_params[index].editor.radio.titles)
            result.print(encodeString(_params[index].editor.radio.titles[i]));
          else
            result.print(encodeString(_params[index].editor.radio.values[i]));
          result.print('\n');
        }
      }
    } else if (_params[index].editor.type == editor_t::EDIT_TEXTAREA) {
      result.print("<textarea name=\"");
      result.print(_params[index].name);
      result.print('"');
      if (_params[index].editor.textarea.cols) {
        result.print(" cols=");
        result.print(_params[index].editor.textarea.cols);
      }
      if (_params[index].editor.textarea.rows) {
        result.print(" rows=");
        result.print(_params[index].editor.textarea.rows);
      }
      if (_params[index].editor.textarea.maxlength) {
        result.print(MAXLENGTH_PSTR);
        result.print(_params[index].editor.textarea.maxlength);
      }
      if (_params[index].editor.disabled) {
        result.print(DISABLED_PSTR);
      }
      if (_params[index].editor.required) {
        result.print(REQUIRED_PSTR);
      }
      if (_params[index].editor.readonly) {
        result.print(READONLY_PSTR);
      }
      result.print(" onkeydown=\"processTab(this,event);\">");
      toStream(index, result, true);
      result.print("</textarea>");
    } else {
      result.print("<input type=\"");
      if (_params[index].editor.type == editor_t::EDIT_TEXT)
        result.print("text");
      else if (_params[index].editor.type == editor_t::EDIT_PASSWORD)
        result.print("password");
      else if (_params[index].editor.type == editor_t::EDIT_CHECKBOX)
        result.print("checkbox");
      else if (_params[index].editor.type == editor_t::EDIT_HIDDEN)
        result.print("hidden");
      result.print("\" name=\"");
      result.print(_params[index].name);
      result.print('"');
      if (_params[index].editor.type != editor_t::EDIT_HIDDEN) {
        if (_params[index].editor.disabled) {
          result.print(DISABLED_PSTR);
        }
        if (_params[index].editor.required) {
          result.print(REQUIRED_PSTR);
        }
      }
      if ((_params[index].editor.type == editor_t::EDIT_TEXT) || (_params[index].editor.type == editor_t::EDIT_PASSWORD)) {
        if (_params[index].editor.text.size) {
          result.print(SIZE_PSTR);
          result.print(_params[index].editor.text.size);
        }
        if (_params[index].editor.text.maxlength) {
          result.print(MAXLENGTH_PSTR);
          result.print(_params[index].editor.text.maxlength);
        }
        if (_params[index].editor.readonly) {
          result.print(READONLY_PSTR);
        }
      } else if (_params[index].editor.type == editor_t::EDIT_CHECKBOX) {
        result.print(" value=\"");
        result.print(encodeString(_params[index].editor.checkbox.checkedvalue));
        result.print('"');
        if (toString(index).equals(_params[index].editor.checkbox.checkedvalue)) {
          result.print(CHECKED_PSTR);
        }
        if (_params[index].editor.readonly) {
          result.print(" onclick=\"return false;\"");
        } else {
          result.print(" onchange=\"document.getElementsByName('");
          result.print(_params[index].name);
          result.print("')[1].disabled=this.checked;\"");
        }
        result.print("><input type=\"hidden\" name=\"");
        result.print(_params[index].name);
        result.print("\" value=\"");
        result.print(encodeString(_params[index].editor.checkbox.uncheckedvalue));
        result.print('"');
        if (toString(index).equals(_params[index].editor.checkbox.checkedvalue)) {
          result.print(DISABLED_PSTR);
        }
        result.print('>');
      }
      if ((_params[index].editor.type == editor_t::EDIT_TEXT) || (_params[index].editor.type == editor_t::EDIT_PASSWORD) ||
        (_params[index].editor.type == editor_t::EDIT_HIDDEN)) {
        result.print(" value=\"");
        toStream(index, result, true);
        result.print('"');
        if (_params[index].editor.type == editor_t::EDIT_TEXT) {
          if (! _params[index].editor.readonly) {
            if ((_params[index].type >= paraminfo_t::PARAM_I8) && (_params[index].type <= paraminfo_t::PARAM_U32)) { // Integers
              result.print(" onblur=\"checkInt(this,");
              if ((_params[index].type == paraminfo_t::PARAM_I8) || (_params[index].type == paraminfo_t::PARAM_I16) ||
                (_params[index].type == paraminfo_t::PARAM_I32)) {
                result.printf(FMT_I32_PSTR, _params[index].defvalue.asint);
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf(FMT_U32_PSTR, _params[index].defvalue.asuint);
              }
              result.print(',');
              if ((_params[index].type == paraminfo_t::PARAM_I8) || (_params[index].type == paraminfo_t::PARAM_I16) ||
                (_params[index].type == paraminfo_t::PARAM_I32)) {
                result.printf(FMT_I32_PSTR, _params[index].minvalue.asint);
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf(FMT_U32_PSTR, _params[index].minvalue.asuint);
              }
              result.print(',');
              if ((_params[index].type == paraminfo_t::PARAM_I8) || (_params[index].type == paraminfo_t::PARAM_I16) ||
                (_params[index].type == paraminfo_t::PARAM_I32)) {
                result.printf(FMT_I32_PSTR, _params[index].maxvalue.asint);
              } else { // type == PARAM_U8 or PARAM_U16 or PARAM_U32
                result.printf(FMT_U32_PSTR, _params[index].maxvalue.asuint);
              }
              result.print(");\"");
            } else if (_params[index].type == paraminfo_t::PARAM_FLOAT) {
              result.print(" onblur=\"checkFloat(this,");
              if (isnan(_params[index].defvalue.asfloat)) {
                result.print(NAN_PSTR);
              } else {
                result.printf(FMT_FLOAT_PSTR, _params[index].defvalue.asfloat);
              }
              result.print(',');
              if (isnan(_params[index].minvalue.asfloat)) {
                result.print(NAN_PSTR);
              } else {
                result.printf(FMT_FLOAT_PSTR, _params[index].minvalue.asfloat);
              }
              result.print(',');
              if (isnan(_params[index].maxvalue.asfloat)) {
                result.print(NAN_PSTR);
              } else {
                result.printf(FMT_FLOAT_PSTR, _params[index].maxvalue.asfloat);
              }
              result.print(");\"");
            }
          }
        }
        result.print('>');
      }
    }
    return result;
  }
  return String();
}
#endif

#ifdef ESP8266
void Parameters::handleWebPage(ESP8266WebServer &http, const char *restartPath, bool confirmation) {
  String page;

  if (http.method() == HTTP_GET) {
    http.setContentLength(CONTENT_LENGTH_UNKNOWN);
    http.send(200, FPSTR(TEXTHTML_PSTR), FPSTR(EMPTY_PSTR));
    page = F("<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Parameters</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "tr td:first-child{text-align:right;}\n"
      "textarea{resize:none;}\n"
      "</style>\n");
    page.concat(getScript());
    page.concat(F("</head>\n"
      "<body>\n"
      "<form action=\"\" method=\"post\">\n"
      "<table cols=2>\n"));
    http.sendContent(page);
    for (uint16_t i = 0; i < _count; ++i) {
      if (pgm_read_byte(&_params[i].editor.type) == editor_t::EDIT_NONE)
        continue;
      page = F("<tr><td>");
      if (pgm_read_ptr(&_params[i].title))
        page.concat(encodeString((char*)pgm_read_ptr(&_params[i].title)));
      else
        page.concat(FPSTR((char*)pgm_read_ptr(&_params[i].name)));
      page.concat(F("</td><td>"));
      page.concat(getEditor(i));
      page.concat(F("</td></tr>\n"));
      http.sendContent(page);
    }
    page = F("</table>\n"
      "<p>\n"
      "<input type=\"submit\" value=\"Store\">\n"
      "<input type=\"button\" value=\"Clear\" onclick=\"");
    if (confirmation) {
      page.concat(F("if(confirm('Are you sure to clear parameters?'))"));
    }
    page.concat(F("{openUrl('"));
    page.concat(http.uri());
    page.concat(F("','delete');location.reload();}\">\n"));
    if (restartPath) {
      page.concat(F("<input type=\"button\" value=\"Restart!\" onclick=\""));
      if (confirmation) {
        page.concat(F("if(confirm('Are you sure to restart?'))"));
      }
      page.concat(F("{location.href='"));
      page.concat(FPSTR(restartPath));
      page.concat(F("';}\">\n"));
    }
    page.concat(F("</form>\n"
      "</body>\n"
      "</html>\n"));
    http.sendContent(page);
    http.sendContent(FPSTR(EMPTY_PSTR));
    http.client().stop();
  } else if (http.method() == HTTP_POST) {
    String errors;

    page = F("<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Store parameters</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "<meta http-equiv=\"refresh\" content=\"5;URL=");
    page.concat(http.uri());
    page.concat(F("\">\n"
      "</head>\n"
      "<body>\n"));
    for (uint16_t i = 0; i < http.args(); ++i) {
      int16_t param = find(http.argName(i).c_str());

      if (param >= 0) {
        if (! fromString(param, http.arg(i))) {
          errors.concat(F("Error setting parameter \""));
          errors.concat(http.argName(i));
          errors.concat(F("\"!<br>\n"));
        }
      }
    }
    if (! update()) {
      errors.concat(F("Error storing EEPROM parameters!\n"));
    }
    if (errors.isEmpty()) {
      page.concat(F("OK\n"));
    } else {
      page.concat(F("<span style=\"color:red\">\n"));
      page.concat(errors);
      page.concat(F("</span>\n"));
    }
    page.concat(F("<p>\n"
      "Wait for 5 sec. or click <a href=\""));
    page.concat(http.uri());
    page.concat(F("\">this</a> to return to previous page\n"
      "</body>\n"
      "</html>\n"));
    http.send(errors.isEmpty() ? 200 : 400, FPSTR(TEXTHTML_PSTR), page);
  } else if (http.method() == HTTP_DELETE) {
    if (clear()) {
      http.send(200, FPSTR(TEXTPLAIN_PSTR), F("OK"));
    } else {
      http.send(400, FPSTR(TEXTPLAIN_PSTR), F("Error clearing EEPROM parameters!"));
    }
  }
}

#else
void Parameters::handleWebPage(WebServer &http, const char *restartPath, bool confirmation) {
  String page;

  if (http.method() == HTTP_GET) {
    http.setContentLength(CONTENT_LENGTH_UNKNOWN);
    http.send(200, TEXTHTML_PSTR, EMPTY_PSTR);
    page = "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Parameters</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "tr td:first-child{text-align:right;}\n"
      "textarea{resize:none;}\n"
      "</style>\n";
    page.concat(getScript());
    page.concat("</head>\n"
      "<body>\n"
      "<form action=\"\" method=\"post\">\n"
      "<table cols=2>\n");
    http.sendContent(page);
    for (uint16_t i = 0; i < _count; ++i) {
      if (_params[i].editor.type == editor_t::EDIT_NONE)
        continue;
      page = "<tr><td>";
      if (_params[i].title)
        page.concat(encodeString(_params[i].title));
      else
        page.concat(_params[i].name);
      page.concat("</td><td>");
      page.concat(getEditor(i));
      page.concat("</td></tr>\n");
      http.sendContent(page);
    }
    page = "</table>\n"
      "<p>\n"
      "<input type=\"submit\" value=\"Store\">\n"
      "<input type=\"button\" value=\"Clear\" onclick=\"if(confirm('Are you sure to clear parameters?')){openUrl('";
    page.concat(http.uri());
    page.concat("','delete');location.reload();}\">\n");
    if (restartPath) {
      page.concat("<input type=\"button\" value=\"Restart!\" onclick=\"if(confirm('Are you sure to restart?')){location.href='");
      page.concat(restartPath);
      page.concat("';}\">\n");
    }
    page.concat("</form>\n"
      "</body>\n"
      "</html>\n");
    http.sendContent(page);
    http.sendContent(EMPTY_PSTR);
    http.client().stop();
  } else if (http.method() == HTTP_POST) {
    String errors;

    page = "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Store parameters</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "<meta http-equiv=\"refresh\" content=\"5;URL=";
    page.concat(http.uri());
    page.concat("\">\n"
      "</head>\n"
      "<body>\n");
    for (uint16_t i = 0; i < http.args(); ++i) {
      int16_t param = find(http.argName(i).c_str());

      if (param >= 0) {
        if (! fromString(param, http.arg(i))) {
          errors.concat("Error setting parameter \"");
          errors.concat(http.argName(i));
          errors.concat("\"!<br>\n");
        }
      }
    }
    if (! update()) {
      errors.concat("Error storing EEPROM parameters!\n");
    }
    if (errors.isEmpty()) {
      page.concat("OK\n");
    } else {
      page.concat("<span style=\"color:red\">\n");
      page.concat(errors);
      page.concat("</span>\n");
    }
    page.concat("<p>\n"
      "Wait for 5 sec. or click <a href=\"");
    page.concat(http.uri());
    page.concat("\">this</a> to return to previous page\n"
      "</body>\n"
      "</html>\n");
    http.send(errors.isEmpty() ? 200 : 400, TEXTHTML_PSTR, page);
  } else if (http.method() == HTTP_DELETE) {
    if (clear()) {
      http.send(200, TEXTPLAIN_PSTR, "OK");
    } else {
      http.send(400, TEXTPLAIN_PSTR, "Error clearing EEPROM parameters!");
    }
  }
}
#endif

uint16_t Parameters::crc16(uint8_t data, uint16_t crc) {
  crc ^= data << 8;
  for (uint8_t i = 0; i < 8; ++i)
    crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
  return crc;
}

uint16_t Parameters::crc16(const uint8_t *data, uint16_t size, uint16_t crc) {
  while (size--) {
    crc ^= *data++ << 8;
    for (uint8_t i = 0; i < 8; ++i)
      crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

void *Parameters::getPtr(uint16_t index) const {
  void *result = NULL;

  if (_inited && (index < _count)) {
    result = EEPROM.getDataPtr() + sizeof(header_t);
    for (uint16_t i = 0; i < index; ++i) {
#ifdef ESP8266
      result = result + pgm_read_word(&_params[i].size);
#else
      result = result + _params[i].size;
#endif
    }
  }
  return result;
}

bool Parameters::check() {
  if (_inited) {
    uint8_t *ptr = EEPROM.getDataPtr();
    const header_t *header = (header_t*)ptr;

    if (header->sign == EEPROM_SIGN) {
      uint16_t crc = 0xFFFF;

      ptr += sizeof(header_t);
      for (uint16_t i = 0; i < _count; ++i) {
#ifdef ESP8266
        crc = crc16(ptr, pgm_read_word(&_params[i].size), crc);
        ptr += pgm_read_word(&_params[i].size);
#else
        crc = crc16(ptr, _params[i].size, crc);
        ptr += _params[i].size;
#endif
      }
      return crc == header->crc;
    }
  }
  return false;
}

bool Parameters::update() {
  if (! _inited)
    return false;

  uint8_t *ptr = EEPROM.getDataPtr();
  header_t *header = (header_t*)ptr;
  uint16_t crc = 0xFFFF;

  ptr += sizeof(header_t);
  for (uint16_t i = 0; i < _count; ++i) {
#ifdef ESP8266
    crc = crc16(ptr, pgm_read_word(&_params[i].size), crc);
    ptr += pgm_read_word(&_params[i].size);
#else
    crc = crc16(ptr, _params[i].size, crc);
    ptr += _params[i].size;
#endif
  }
  if ((header->sign != EEPROM_SIGN) || (header->crc != crc)) {
    header->sign = EEPROM_SIGN;
    header->crc = crc;
    return EEPROM.commit();
  }
  return true;
}

String Parameters::encodeString(char c) {
  String result;

  if (c == '"')
#ifdef ESP8266
    result = FPSTR(QUOT_PSTR);
#else
    result = QUOT_PSTR;
#endif
  else if (c == '<')
#ifdef ESP8266
    result = FPSTR(LT_PSTR);
#else
    result = LT_PSTR;
#endif
  else if (c == '>')
#ifdef ESP8266
    result = FPSTR(GT_PSTR);
#else
    result = GT_PSTR;
#endif
  else
    result = c;
  return result;
}

String Parameters::encodeString(const char *str) {
  String result;

  if (str) {
#ifdef ESP8266
    uint16_t len = strlen_P(str);
#else
    uint16_t len = strlen(str);
#endif

    if (result.reserve(len)) {
      for (uint16_t i = 0; i < len; ++i) {
#ifdef ESP8266
        char c = pgm_read_byte(&str[i]);

        if (c == '"')
          result.concat(FPSTR(QUOT_PSTR));
        else if (c == '<')
          result.concat(FPSTR(LT_PSTR));
        else if (c == '>')
          result.concat(FPSTR(GT_PSTR));
        else
          result.concat(c);
#else
        if (str[i] == '"')
          result.concat(QUOT_PSTR);
        else if (str[i] == '<')
          result.concat(LT_PSTR);
        else if (str[i] == '>')
          result.concat(GT_PSTR);
        else
          result.concat(str[i]);
#endif
      }
    }
  }
  return result;
}

static uint8_t findFreeChannel() {
  uint8_t result = 0;
  int16_t networks;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  networks = WiFi.scanNetworks(false, true);
  if (networks > 0) {
    int8_t signals[13];

    memset(signals, -128, sizeof(signals));
    for (uint8_t i = 0; i < networks; ++i) {
      uint8_t channel = WiFi.channel(i);
      int8_t rssi = WiFi.RSSI(i);

      if (rssi > signals[channel - 1])
        signals[channel - 1] = rssi;
    }
    for (uint8_t i = 1; i < 13; ++i) {
      if (signals[i] < signals[result])
        result = i;
    }
  }
  WiFi.scanDelete();
  return result + 1;
}

bool paramsCaptivePortal(Parameters *params, const char *ssid, const char *pswd, uint16_t duration, cpcallback_t callback) {
#ifdef ESP8266
  static const char SLASH_PSTR[] PROGMEM = "/";
  static const char RESTART_PSTR[] PROGMEM = "/restart";
  static const char GENERATE204_PSTR[] PROGMEM = "/generate_204";
#else
  static const char SLASH_PSTR[] = "/";
  static const char RESTART_PSTR[] = "/restart";
  static const char GENERATE204_PSTR[] = "/generate_204";
#endif

  {
    uint8_t channel = findFreeChannel();

    WiFi.mode(WIFI_AP);
#ifdef ESP8266
    {
      char _ssid[strlen_P(ssid) + 1], _pswd[strlen_P(pswd) + 1];

      strcpy_P(_ssid, ssid);
      strcpy_P(_pswd, pswd);
      if (! WiFi.softAP(_ssid, _pswd, channel))
        return false;
    }
#else
    if (! WiFi.softAP(ssid, pswd, channel))
      return false;
#endif
  }
  if (callback) {
    callback(CP_INIT, &WiFi);
  }

  DNSServer *dns;
#ifdef ESP8266
  ESP8266WebServer *http;
#else
  WebServer *http;
#endif

  dns = new DNSServer();
  if (! dns) {
    WiFi.softAPdisconnect(true);
    return false;
  }
  dns->setErrorReplyCode(DNSReplyCode::NoError);
#ifdef ESP8266
  if (! dns->start(53, F("*"), WiFi.softAPIP())) {
#else
  if (! dns->start(53, "*", WiFi.softAPIP())) {
#endif
    delete dns;
    WiFi.softAPdisconnect(true);
    return false;
  }

#ifdef ESP8266
  http = new ESP8266WebServer();
#else
  http = new WebServer();
#endif
  if (! http) {
    delete dns;
    WiFi.softAPdisconnect(true);
    return false;
  }
  http->onNotFound([&]() {
    if (! http->hostHeader().equals(WiFi.softAPIP().toString())) {
#ifdef ESP8266
      http->sendHeader(F("Location"), String(F("http://")) + WiFi.softAPIP().toString(), true);
      http->send(302, FPSTR(TEXTPLAIN_PSTR), FPSTR(EMPTY_PSTR));
#else
      http->sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
      http->send(302, TEXTPLAIN_PSTR, EMPTY_PSTR);
#endif
      return;
    }

    String page;

#ifdef ESP8266
    page = F("<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Page Not Found!</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "</head>\n"
      "<body>\n"
      "Page <b>");
    page.concat(http->uri());
    page.concat(F("</b> not found!\n"
      "</body>\n"
      "</html>\n"));
    http->send(404, FPSTR(TEXTHTML_PSTR), page);
#else
    page = "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Page Not Found!</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "</head>\n"
      "<body>\n"
      "Page <b>";
    page.concat(http->uri());
    page.concat("</b> not found!\n"
      "</body>\n"
      "</html>\n");
    http->send(404, TEXTHTML_PSTR, page);
#endif
  });
#ifdef ESP8266
  http->on(FPSTR(SLASH_PSTR), [&]() {
    params->handleWebPage(*http, RESTART_PSTR);
#else
  http->on(SLASH_PSTR, [&]() {
    params->handleWebPage(*http, RESTART_PSTR);
#endif
  });
#ifdef ESP8266
  http->on(FPSTR(GENERATE204_PSTR), [&]() {
    params->handleWebPage(*http, RESTART_PSTR, false);
#else
  http->on(GENERATE204_PSTR, [&]() {
    params->handleWebPage(*http, RESTART_PSTR, false);
#endif
  });
#ifdef ESP8266
  http->on(FPSTR(RESTART_PSTR), HTTP_GET, [&]() {
    http->send_P(200, TEXTHTML_PSTR, PSTR("<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Restart</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "<meta http-equiv=\"refresh\" content=\"15;URL=/\">\n"
      "</head>\n"
      "<body>\n"
      "Restarting...\n"
      "</body>\n"
      "</html>"));
#else
  http->on(RESTART_PSTR, HTTP_GET, [&]() {
    http->send(200, TEXTHTML_PSTR, "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Restart</title>\n"
      "<style>\n"
      "body{background-color:#eee;}\n"
      "</style>\n"
      "<meta http-equiv=\"refresh\" content=\"15;URL=/\">\n"
      "</head>\n"
      "<body>\n"
      "Restarting...\n"
      "</body>\n"
      "</html>");
#endif
    http->client().stop();
    if (callback) {
      callback(CP_RESTART, NULL);
    }
    ESP.restart();
  });
  if (callback) {
    callback(CP_WEB, http);
  }
  http->begin();

  uint8_t clients = 0;

  if (duration) {
    uint32_t start = millis();

    while (millis() - start < duration * 1000) {
      uint8_t cln = WiFi.softAPgetStationNum();

      if (cln != clients) {
        if (cln && (! clients)) { // First client connected
          if (callback) {
            callback(CP_CONNECT, &WiFi);
          }
        } else if ((! cln) && clients) { // Last client disconnected
          if (callback) {
            callback(CP_DISCONNECT, &WiFi);
          }
        }
        clients = cln;
      }
      if (cln)
        start = millis();
      dns->processNextRequest();
      http->handleClient();
      if (callback) {
        callback(CP_IDLE, &WiFi);
      }
      delay(1);
    }
  } else {
    for (;;) {
      uint8_t cln = WiFi.softAPgetStationNum();

      if (cln != clients) {
        if (cln && (! clients)) { // First client connected
          if (callback) {
            callback(CP_CONNECT, &WiFi);
          }
        } else if ((! cln) && clients) { // Last client disconnected
          if (callback) {
            callback(CP_DISCONNECT, &WiFi);
          }
        }
        clients = cln;
      }
      dns->processNextRequest();
      http->handleClient();
      if (callback) {
        callback(CP_IDLE, &WiFi);
      }
      delay(1);
    }
  }
  delete http;
  delete dns;
  WiFi.softAPdisconnect();
  if (callback) {
    callback(CP_DONE, &WiFi);
  }
  return true;
}
