#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <PubSubClient.h>
#include "Parameters.h"
#include "RtcFlags.h"

const uint8_t RELAY_PIN = 0;
const bool RELAY_LEVEL = HIGH;

const uint8_t LED_PIN = 2;
const bool LED_LEVEL = LOW;

const uint32_t LED_PULSE = 25; // 25 ms.

const char CP_SSID[] PROGMEM = "ESP01_Relay";
const char CP_PSWD[] PROGMEM = "1029384756";

const char PARAM_WIFI_SSID_NAME[] PROGMEM = "wifi_ssid";
const char PARAM_WIFI_SSID_TITLE[] PROGMEM = "WiFi SSID";
const char PARAM_WIFI_PSWD_NAME[] PROGMEM = "wifi_pswd";
const char PARAM_WIFI_PSWD_TITLE[] PROGMEM = "WiFi password";
const char PARAM_MQTT_SERVER_NAME[] PROGMEM = "mqtt_server";
const char PARAM_MQTT_SERVER_TITLE[] PROGMEM = "MQTT broker";
const char PARAM_MQTT_PORT_NAME[] PROGMEM = "mqtt_port";
const char PARAM_MQTT_PORT_TITLE[] PROGMEM = "MQTT port";
const uint16_t PARAM_MQTT_PORT_DEF = 1883;
const char PARAM_MQTT_CLIENT_NAME[] PROGMEM = "mqtt_client";
const char PARAM_MQTT_CLIENT_TITLE[] PROGMEM = "MQTT client";
const char PARAM_MQTT_CLIENT_DEF[] PROGMEM = "ESP01_Relay";
const char PARAM_MQTT_USER_NAME[] PROGMEM = "mqtt_user";
const char PARAM_MQTT_USER_TITLE[] PROGMEM = "MQTT user";
const char PARAM_MQTT_PSWD_NAME[] PROGMEM = "mqtt_pswd";
const char PARAM_MQTT_PSWD_TITLE[] PROGMEM = "MQTT password";
const char PARAM_MQTT_TOPIC_NAME[] PROGMEM = "mqtt_topic";
const char PARAM_MQTT_TOPIC_TITLE[] PROGMEM = "MQTT topic";
const char PARAM_MQTT_TOPIC_DEF[] PROGMEM = "/Relay";
const char PARAM_MQTT_RETAINED_NAME[] PROGMEM = "mqtt_retain";
const char PARAM_MQTT_RETAINED_TITLE[] PROGMEM = "MQTT retained";
const bool PARAM_MQTT_RETAINED_DEF = false;
const char PARAM_BOOT_STATE_NAME[] PROGMEM = "boot_state";
const char PARAM_BOOT_STATE_TITLE[] PROGMEM = "Relay state on boot";
const bool PARAM_BOOT_STATE_DEF = false;
const char PARAM_PERSISTENT_NAME[] PROGMEM = "persist";
const char PARAM_PERSISTENT_TITLE[] PROGMEM = "Persistent relay state";
const bool PARAM_PERSISTENT_DEF = false;

const char OFF_PSTR[] PROGMEM = "OFF";
const char ON_PSTR[] PROGMEM = "ON";
const char *const STATES[] PROGMEM = { OFF_PSTR, ON_PSTR };

const paraminfo_t PARAMS[] PROGMEM = {
  PARAM_STR(PARAM_WIFI_SSID_NAME, PARAM_WIFI_SSID_TITLE, 33, NULL),
  PARAM_PASSWORD(PARAM_WIFI_PSWD_NAME, PARAM_WIFI_PSWD_TITLE, 33, NULL),
  PARAM_STR(PARAM_MQTT_SERVER_NAME, PARAM_MQTT_SERVER_TITLE, 33, NULL),
  PARAM_U16(PARAM_MQTT_PORT_NAME, PARAM_MQTT_PORT_TITLE, PARAM_MQTT_PORT_DEF),
  PARAM_STR(PARAM_MQTT_CLIENT_NAME, PARAM_MQTT_CLIENT_TITLE, 33, PARAM_MQTT_CLIENT_DEF),
  PARAM_STR(PARAM_MQTT_USER_NAME, PARAM_MQTT_USER_TITLE, 33, NULL),
  PARAM_PASSWORD(PARAM_MQTT_PSWD_NAME, PARAM_MQTT_PSWD_TITLE, 33, NULL),
  PARAM_STR(PARAM_MQTT_TOPIC_NAME, PARAM_MQTT_TOPIC_TITLE, 33, PARAM_MQTT_TOPIC_DEF),
  PARAM_BOOL(PARAM_MQTT_RETAINED_NAME, PARAM_MQTT_RETAINED_TITLE, PARAM_MQTT_RETAINED_DEF),
  PARAM_BOOL_CUSTOM(PARAM_BOOT_STATE_NAME, PARAM_BOOT_STATE_TITLE, PARAM_BOOT_STATE_DEF, EDITOR_RADIO(2, BOOLS, STATES, false, false, false)),
  PARAM_BOOL(PARAM_PERSISTENT_NAME, PARAM_PERSISTENT_TITLE, PARAM_PERSISTENT_DEF)
};

Parameters *params = NULL;
ESP8266WebServer *http = NULL;
WiFiClient *client = NULL;
PubSubClient *mqtt = NULL;
bool relayState;

static void halt(const char *msg = NULL) {
  if (msg)
    Serial.println(FPSTR(msg));
  Serial.flush();
  ESP.deepSleep(0);
}

static void restart(const char *msg = NULL) {
  if (msg)
    Serial.println(FPSTR(msg));
  Serial.flush();
  ESP.restart();
}

static void relaySwitch(bool on, bool publish = false) {
  digitalWrite(RELAY_PIN, on == RELAY_LEVEL);
  if (publish && mqtt && mqtt->connected()) {
    char value;

    value = '0' + on;
    mqtt->publish((char*)params->value(PARAM_MQTT_TOPIC_NAME), (uint8_t*)&value, 1, *(bool*)params->value(PARAM_MQTT_RETAINED_NAME));
  }
  relayState = on;
  if (*(bool*)params->value(PARAM_PERSISTENT_NAME)) {
    params->set(PARAM_BOOT_STATE_NAME, &relayState);
    params->update();
  }
}

static void httpPageNotFound() {
  http->send_P(404, PSTR("text/plain"), PSTR("Page Not Found!"));
}

static void httpRootPage() {
  String page;

  page = F("<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "<title>ESP01-Relay</title>\n"
    "<style>\n"
    "body{background-color:#eee;}\n"
    ".checkbox{vertical-align:top;margin:0 3px 0 0;width:17px;height:17px;}\n"
    ".checkbox+label{cursor:pointer;}\n"
    ".checkbox:not(checked){position:absolute;opacity:0;}\n"
    ".checkbox:not(checked)+label{position:relative;padding:0 0 0 60px;}\n"
    ".checkbox:not(checked)+label:before{content:'';position:absolute;top:-4px;left:0;width:50px;height:26px;border-radius:13px;background:#CDD1DA;box-shadow:inset 0 2px 3px rgba(0,0,0,.2);}\n"
    ".checkbox:not(checked)+label:after{content:'';position:absolute;top:-2px;left:2px;width:22px;height:22px;border-radius:10px;background:#FFF;box-shadow:0 2px 5px rgba(0,0,0,.3);transition:all .2s;}\n"
    ".checkbox:checked+label:before{background:#9FD468;}\n"
    ".checkbox:checked+label:after{left:26px;}\n"
    "</style>\n"
    "<script type=\"text/javascript\">\n"
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
    "//alert(x.responseText);\n"
    "return false;\n"
    "}\n"
    "return true;\n"
    "}\n"
    "function refreshData(){\n"
    "let request=getXmlHttpRequest();\n"
    "request.open('GET','/switch?dummy='+Date.now(),true);\n"
    "request.onreadystatechange=function(){\n"
    "if((request.readyState==4)&&(request.status==200)){\n"
    "let data=JSON.parse(request.responseText);\n"
    "document.getElementById('relay').checked=data.state;\n"
    "}\n"
    "}\n"
    "request.send(null);\n"
    "}\n"
    "setInterval(refreshData,500);\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<input type=\"checkbox\" class=\"checkbox\" name=\"relay\" id=\"relay\" onchange=\"openUrl('/switch?on='+this.checked+'&dummy='+Date.now(),'post');\"");
  if (relayState)
    page.concat(F(" checked"));
  page.concat(F(">\n"
    "<label for=\"relay\">Relay</label>\n"
    "<p>\n"
    "<button onclick=\"location.href='/setup'\">Setup</button>\n"
    "<button onclick=\"if(confirm('Are you sure to restart?')){location.href='/restart';}\">Restart!</button>\n"
    "</body>\n"
    "</html>"));
  http->send(200, F("text/html"), page);
}

static void httpSwitchPage() {
  if (http->method() == HTTP_GET) {
    String page = F("{\"state\":");

    page.concat(FPSTR(BOOLS[relayState]));
    page.concat('}');
    http->send(200, F("text/json"), page);
  } else if (http->method() == HTTP_POST) {
    bool error = true;

    if (http->hasArg(F("on"))) {
      String param = http->arg(F("on"));

      if (param.equals(FPSTR(BOOLS[false])) || param.equals(FPSTR(BOOLS[true]))) {
        relaySwitch(param.equals(FPSTR(BOOLS[true])), true);
        error = false;
      }
    }
    http->send_P(error ? 400 : 200, PSTR("text/plain"), error ? PSTR("Bad argument!") : PSTR("OK"));
  } else {
    http->send_P(405, PSTR("text/plain"), PSTR("Method Not Allowed!"));
  }
}

static void httpRestartPage() {
  http->send_P(200, PSTR("text/html"), PSTR("<!DOCTYPE html>\n"
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
  http->client().stop();
  restart(PSTR("Restarting..."));
}

void setup() {
  WiFi.persistent(false);

  Serial.begin(115200);
  Serial.println();

  params = new Parameters(PARAMS, ARRAY_SIZE(PARAMS));
  if ((! params) || (! params->begin()))
    halt(PSTR("Initialization of parameters FAIL!"));

  relayState = *(bool*)params->value(PARAM_BOOT_STATE_NAME);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, relayState == RELAY_LEVEL);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ! LED_LEVEL);

  bool paramIncomplete = (! *(char*)params->value(PARAM_WIFI_SSID_NAME)) || (! *(char*)params->value(PARAM_WIFI_PSWD_NAME));

  if (paramIncomplete || ((ESP.getResetInfoPtr()->reason != REASON_SOFT_RESTART) && (! RtcFlags::getFlag(0)))) {
    RtcFlags::setFlag(0);
    if (! paramsCaptivePortal(params, CP_SSID, CP_PSWD, paramIncomplete ? 0 : 60, [&](cpevent_t event, void *param) {
      switch (event) {
        case CP_INIT:
          Serial.print(F("Captive portal \""));
          Serial.print(((ESP8266WiFiClass*)param)->softAPSSID());
          Serial.print(F("\" with password \""));
          Serial.print(((ESP8266WiFiClass*)param)->softAPPSK());
          Serial.println(F("\" started"));
          Serial.print(F("Visit to http://"));
          Serial.println(((ESP8266WiFiClass*)param)->softAPIP());
          break;
        case CP_DONE:
          digitalWrite(LED_PIN, ! LED_LEVEL);
          Serial.println(F("Captive portal closed"));
          break;
        case CP_RESTART:
          digitalWrite(LED_PIN, ! LED_LEVEL);
          Serial.println(F("Restarting..."));
          Serial.flush();
          break;
        case CP_IDLE:
          digitalWrite(LED_PIN, (millis() % (((ESP8266WiFiClass*)param)->softAPgetStationNum() > 0 ? 500 : 250) < LED_PULSE) == LED_LEVEL);
          break;
        default:
          break;
      }
    }))
    halt(PSTR("Captive portal FAIL!"));

    relayState = *(bool*)params->value(PARAM_BOOT_STATE_NAME);
    digitalWrite(RELAY_PIN, relayState == RELAY_LEVEL);
  }
  RtcFlags::clearFlag(0);
  if ((! *(char*)params->value(PARAM_WIFI_SSID_NAME)) || (! *(char*)params->value(PARAM_WIFI_PSWD_NAME)))
    restart(PSTR("Parameters incomplete!"));

  http = new ESP8266WebServer();
  if (! http)
    halt(PSTR("Web server initialization FAIL!"));
  http->onNotFound(httpPageNotFound);
  http->on(F("/"), HTTP_GET, httpRootPage);
  http->on(F("/index.html"), HTTP_GET, httpRootPage);
  http->on(F("/switch"), httpSwitchPage);
  http->on(F("/setup"), [&]() {
    params->handleWebPage(*http);
  });
  http->on(F("/restart"), HTTP_GET, httpRestartPage);
  http->on(F("/description.xml"), HTTP_GET, [&]() {
    SSDP.schema(http->client());
  });

  SSDP.setSchemaURL(F("description.xml"));
  SSDP.setHTTPPort(80);
  SSDP.setName(F("ESP01 Relay"));
  SSDP.setSerialNumber(F("12345678"));
  SSDP.setURL(F("index.html"));
  SSDP.setModelName(F("ESP01S Relay"));
  SSDP.setModelNumber(F("v1.0"));
  SSDP.setManufacturer(F("NoName Ltd."));
  SSDP.setDeviceType(F("upnp:rootdevice"));

  if (*(char*)params->value(PARAM_MQTT_SERVER_NAME) && *(char*)params->value(PARAM_MQTT_CLIENT_NAME) && *(char*)params->value(PARAM_MQTT_TOPIC_NAME)) {
    client = new WiFiClient();
    if (! client)
      halt(PSTR("WiFi client creation FAIL!"));
    mqtt = new PubSubClient(*client);
    if (! mqtt)
      halt(PSTR("MQTT initialization FAIL!"));
    mqtt->setServer((char*)params->value(PARAM_MQTT_SERVER_NAME), *(uint16_t*)params->value(PARAM_MQTT_PORT_NAME));
    mqtt->setCallback([&](char *topic, uint8_t *payload, unsigned int length) {
      if (! strcmp(topic, (char*)params->value(PARAM_MQTT_TOPIC_NAME))) {
        if ((length == 1) && (*payload >= '0') && (*payload <= '1')) {
          relaySwitch(*payload - '0');
        }
      }
    });
  }

  WiFi.mode(WIFI_STA);
  {
    const char *name = (char*)params->value(PARAM_MQTT_CLIENT_NAME);

    if (*name) {
      if (! WiFi.hostname(name)) {
        Serial.println(F("Error setting host name!"));
      }
    }
  }

  Serial.println(F("Relay started"));
}

void loop() {
  const uint32_t WIFI_TIMEOUT = 30000; // 30 sec.
  const uint32_t MQTT_TIMEOUT = 30000; // 30 sec.

  static uint32_t lastWiFiTry = 0;
  static uint32_t lastMqttTry = 0;

  if (! WiFi.isConnected()) {
    if ((! lastWiFiTry) || (millis() - lastWiFiTry >= WIFI_TIMEOUT)) {
      uint32_t start;

      SSDP.end();
      {
        const char *ssid;

        ssid = (char*)params->value(PARAM_WIFI_SSID_NAME);
        WiFi.begin(ssid, (char*)params->value(PARAM_WIFI_PSWD_NAME));
        Serial.print(F("Connecting to SSID \""));
        Serial.print(ssid);
        Serial.print('"');
      }
      start = millis();
      while ((! WiFi.isConnected()) && (millis() - start < WIFI_TIMEOUT)) {
        digitalWrite(LED_PIN, LED_LEVEL);
        delay(LED_PULSE);
        digitalWrite(LED_PIN, ! LED_LEVEL);
        Serial.print('.');
        delay(500 - LED_PULSE);
      }
      if (WiFi.isConnected()) {
        http->begin();
        SSDP.begin();
        Serial.print(F(" OK ("));
        Serial.print(WiFi.localIP());
        Serial.println(')');
        lastWiFiTry = 0;
      } else {
        WiFi.disconnect();
        Serial.println(F(" FAIL!"));
        lastWiFiTry = millis();
      }
    }
  } else {
    http->handleClient();
    if (mqtt) {
      if (! mqtt->connected()) {
        if ((! lastMqttTry) || (millis() - lastMqttTry >= MQTT_TIMEOUT)) {
          const char *user, *pswd;
          bool connected;

          user = (char*)params->value(PARAM_MQTT_USER_NAME);
          pswd = (char*)params->value(PARAM_MQTT_PSWD_NAME);
          digitalWrite(LED_PIN, LED_LEVEL);
          Serial.print(F("Connecting to MQTT broker \""));
          Serial.print((char*)params->value(PARAM_MQTT_SERVER_NAME));
          Serial.print(F("\"... "));
          if (*user && *pswd)
            connected = mqtt->connect((char*)params->value(PARAM_MQTT_CLIENT_NAME), user, pswd);
          else
            connected = mqtt->connect((char*)params->value(PARAM_MQTT_CLIENT_NAME));
          digitalWrite(LED_PIN, ! LED_LEVEL);
          if (connected) {
            Serial.println(F("OK"));
            mqtt->subscribe((char*)params->value(PARAM_MQTT_TOPIC_NAME));
            lastMqttTry = 0;
          } else {
            Serial.println(F("FAIL!"));
            lastMqttTry = millis();
          }
        }
      } else {
        mqtt->loop();
      }
    }
    digitalWrite(LED_PIN, (millis() % 1000 < LED_PULSE) == LED_LEVEL);
  }
//  delay(1);
}
