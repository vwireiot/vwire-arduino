/*
 * Vwire IOT Arduino Library - OTA Addon Implementation
 * 
 * Implements local OTA and Cloud OTA as an optional addon attached to the
 * Vwire core.
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireOTA.h"

#if VWIRE_ENABLE_LOGGING
  #define VWIRE_LOG(message) _vwire->_debugPrint(message)
  #define VWIRE_LOGF(...) _vwire->_debugPrintf(__VA_ARGS__)
#else
  #define VWIRE_LOG(message) do { } while (0)
  #define VWIRE_LOGF(...) do { } while (0)
#endif

#if VWIRE_ENABLE_LOCAL_OTA
  #include <ArduinoOTA.h>
#endif

#if defined(VWIRE_BOARD_ESP32) && VWIRE_ENABLE_CLOUD_OTA
  #include <HTTPUpdate.h>
#elif defined(VWIRE_BOARD_ESP8266) && VWIRE_ENABLE_CLOUD_OTA
  #include <ESP8266httpUpdate.h>
#endif

// =============================================================================
// DEFAULT ADDON INSTANCE
// =============================================================================

static VwireOTAAddon _vwireDefaultOTAAddon;

// =============================================================================
// CONSTRUCTOR / REGISTRATION
// =============================================================================

VwireOTAAddon::VwireOTAAddon()
  : _vwire(nullptr)
  #if VWIRE_ENABLE_LOCAL_OTA
  , _localEnabled(false)
  #endif
  #if VWIRE_ENABLE_CLOUD_OTA
  , _cloudEnabled(false)
  #endif
{}

void VwireOTAAddon::begin(VwireClass& vwire) {
  _vwire = &vwire;
  vwire.addAddon(*this);
}

void VwireOTAAddon::onAttach(VwireClass& vwire) {
  _vwire = &vwire;
}

// =============================================================================
// ADDON LIFECYCLE
// =============================================================================

void VwireOTAAddon::onConnect() {
  #if VWIRE_ENABLE_CLOUD_OTA
  if (_cloudEnabled && _vwire) {
    String otaTopic = _vwire->_buildTopic("ota");
    _vwire->_mqttClient.subscribe(otaTopic.c_str(), 1);
    VWIRE_LOGF("[Vwire] Subscribed to: %s (Cloud OTA)", otaTopic.c_str());
  }
  #endif
}

bool VwireOTAAddon::onMessage(const char* topic, const char* payload) {
  #if VWIRE_ENABLE_CLOUD_OTA
  if (_cloudEnabled) {
    const char* otaPos = strstr(topic, "/ota");
    if (otaPos && *(otaPos + 4) == '\0') {
      _handleCloudOTA(payload);
      return true;
    }
  }
  #else
  (void)topic;
  (void)payload;
  #endif

  return false;
}

void VwireOTAAddon::onRun() {
  #if VWIRE_ENABLE_LOCAL_OTA
  if (_localEnabled) {
    ArduinoOTA.handle();
  }
  #endif
}

#if VWIRE_ENABLE_LOCAL_OTA
// =============================================================================
// LOCAL OTA
// =============================================================================

void VwireOTAAddon::enableLocal(const char* hostname, const char* password) {
  String otaHostname;
  if (hostname) {
    otaHostname = String(hostname);
    if (strlen(_vwire->_hostname) == 0) {
      strncpy(_vwire->_hostname, hostname, sizeof(_vwire->_hostname) - 1);
      _vwire->_hostname[sizeof(_vwire->_hostname) - 1] = '\0';
      if (WiFi.status() == WL_CONNECTED) {
        #if defined(VWIRE_BOARD_ESP32)
        WiFi.setHostname(_vwire->_hostname);
        #elif defined(VWIRE_BOARD_ESP8266)
        WiFi.hostname(_vwire->_hostname);
        #endif
        VWIRE_LOGF("[Vwire] WiFi hostname synced to OTA: %s", _vwire->_hostname);
      }
    }
  } else if (strlen(_vwire->_hostname) > 0) {
    otaHostname = String(_vwire->_hostname);
  } else {
    otaHostname = "vwire-" + String(_vwire->_deviceId).substring(0, 8);
    strncpy(_vwire->_hostname, otaHostname.c_str(), sizeof(_vwire->_hostname) - 1);
    _vwire->_hostname[sizeof(_vwire->_hostname) - 1] = '\0';
  }

  ArduinoOTA.setHostname(otaHostname.c_str());
  VWIRE_LOGF("[Vwire] OTA hostname: %s", otaHostname.c_str());

  if (password) {
    ArduinoOTA.setPassword(password);
  }

  ArduinoOTA.onStart([this]() {
    VWIRE_LOG("[Vwire] OTA Update starting...");
  });
  ArduinoOTA.onEnd([this]() {
    VWIRE_LOG("[Vwire] OTA Update complete!");
  });
  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    VWIRE_LOGF("[Vwire] OTA Progress: %u%%", (progress / (total / 100)));
  });
  ArduinoOTA.onError([this](ota_error_t error) {
    VWIRE_LOGF("[Vwire] OTA Error[%u]", error);
  });

  ArduinoOTA.begin();
  _localEnabled = true;
  VWIRE_LOG("[Vwire] OTA enabled");
}

void VwireOTAAddon::handle() {
  if (_localEnabled) {
    ArduinoOTA.handle();
  }
}
#endif

#if VWIRE_ENABLE_CLOUD_OTA
// =============================================================================
// CLOUD OTA
// =============================================================================

void VwireOTAAddon::enableCloud() {
  _cloudEnabled = true;
  VWIRE_LOG("[Vwire] Cloud OTA enabled");

  if (_vwire->connected()) {
    String otaTopic = _vwire->_buildTopic("ota");
    _vwire->_mqttClient.subscribe(otaTopic.c_str(), 1);
    VWIRE_LOGF("[Vwire] Subscribed to: %s (Cloud OTA)", otaTopic.c_str());
  }
}

void VwireOTAAddon::disableCloud() {
  _cloudEnabled = false;
  VWIRE_LOG("[Vwire] Cloud OTA disabled");
}

bool VwireOTAAddon::isCloudEnabled() const {
  return _cloudEnabled;
}

void VwireOTAAddon::_ensureMqttForOTA() {
  if (_vwire->_mqttClient.connected()) return;

  VWIRE_LOG("[Vwire] MQTT disconnected during OTA download, reconnecting...");
  for (int i = 0; i < 3; i++) {
    _vwire->_setupClient();
    if (_vwire->_connectMQTT()) {
      VWIRE_LOG("[Vwire] MQTT reconnected for OTA status report");
      return;
    }
    delay(1000);
  }

  VWIRE_LOG("[Vwire] MQTT reconnect failed - OTA status may not be reported");
}

void VwireOTAAddon::_publishOTAStatus(const char* updateId, const char* status, int progress,
                                      const char* error) {
  if (!_vwire->connected()) return;

  char topic[96];
  char buffer[256];
  snprintf(topic, sizeof(topic), "vwire/%s/ota_status", _vwire->_deviceId);

  if (error) {
    snprintf(buffer, sizeof(buffer),
             "{\"updateId\":\"%s\",\"status\":\"%s\",\"progress\":%d,\"error\":\"%s\",\"version\":\"%s\"}",
             updateId, status, progress, error, VWIRE_VERSION);
  } else {
    snprintf(buffer, sizeof(buffer),
             "{\"updateId\":\"%s\",\"status\":\"%s\",\"progress\":%d,\"version\":\"%s\"}",
             updateId, status, progress, VWIRE_VERSION);
  }

  _vwire->_mqttClient.publish(topic, buffer, true);
  VWIRE_LOGF("[Vwire] OTA Status: %s %d%%", status, progress);
}

void VwireOTAAddon::_handleCloudOTA(const char* payload) {
  VWIRE_LOG("[Vwire] Cloud OTA command received");

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    VWIRE_LOGF("[Vwire] OTA JSON parse error: %s", err.c_str());
    return;
  }

  const char* url = doc["url"];
  const char* version = doc["version"];
  int size = doc["size"] | 0;
  const char* updateId = doc["updateId"];
  if (!url || !updateId) {
    VWIRE_LOG("[Vwire] OTA command missing required fields");
    return;
  }

  VWIRE_LOGF("[Vwire] OTA: url=%s", url);
  VWIRE_LOGF("[Vwire] OTA: version=%s size=%d", version ? version : "?", size);
  _publishOTAStatus(updateId, "downloading", 0);
  _vwire->_mqttClient.loop();
  delay(100);

  bool useHttps = strncmp(url, "https", 5) == 0;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  if (useHttps) {
    secureClient.setInsecure();
    VWIRE_LOG("[Vwire] OTA: Using HTTPS for firmware download");
  }
  WiFiClient& updateClient = useHttps ? (WiFiClient&)secureClient : plainClient;

  #if defined(VWIRE_BOARD_ESP32)
  #ifdef LED_BUILTIN
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  #endif
  httpUpdate.rebootOnUpdate(false);
  t_httpUpdate_return ret = httpUpdate.update(updateClient, url);
  _ensureMqttForOTA();

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      VWIRE_LOGF("[Vwire] OTA FAILED: (%d) %s",
                           httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      _publishOTAStatus(updateId, "failed", 0, httpUpdate.getLastErrorString().c_str());
      _vwire->_mqttClient.loop();
      delay(200);
      break;
    case HTTP_UPDATE_NO_UPDATES:
      VWIRE_LOG("[Vwire] OTA: No update available");
      _publishOTAStatus(updateId, "failed", 0, "No update available");
      _vwire->_mqttClient.loop();
      delay(200);
      break;
    case HTTP_UPDATE_OK:
      VWIRE_LOG("[Vwire] OTA SUCCESS - Rebooting...");
      _publishOTAStatus(updateId, "completed", 100);
      _vwire->_mqttClient.loop();
      delay(1000);
      ESP.restart();
      break;
  }
  #elif defined(VWIRE_BOARD_ESP8266)
  #ifdef LED_BUILTIN
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
  #endif
  ESPhttpUpdate.rebootOnUpdate(false);
  t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, url);
  _ensureMqttForOTA();

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      VWIRE_LOGF("[Vwire] OTA FAILED: (%d) %s",
                           ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      _publishOTAStatus(updateId, "failed", 0, ESPhttpUpdate.getLastErrorString().c_str());
      _vwire->_mqttClient.loop();
      delay(200);
      break;
    case HTTP_UPDATE_NO_UPDATES:
      VWIRE_LOG("[Vwire] OTA: No update available");
      _publishOTAStatus(updateId, "failed", 0, "No update available");
      _vwire->_mqttClient.loop();
      delay(200);
      break;
    case HTTP_UPDATE_OK:
      VWIRE_LOG("[Vwire] OTA SUCCESS - Rebooting...");
      _publishOTAStatus(updateId, "completed", 100);
      _vwire->_mqttClient.loop();
      delay(1000);
      ESP.restart();
      break;
  }
  #endif
}
#endif

// =============================================================================
// VwireClass BRIDGE METHODS
// =============================================================================

bool VwireClass::_ensureOTAFeature() {
#if VWIRE_ENABLE_LOCAL_OTA || VWIRE_ENABLE_CLOUD_OTA
  if (_otaFeature) {
    return true;
  }

  _otaFeature = &_vwireDefaultOTAAddon;
  _vwireDefaultOTAAddon.begin(*this);
  return true;
#else
  return false;
#endif
}

#if VWIRE_HAS_OTA
void VwireClass::enableOTA(const char* hostname, const char* password) {
  #if VWIRE_ENABLE_LOCAL_OTA
  if (_ensureOTAFeature()) {
    _otaFeature->enableLocal(hostname, password);
  }
  #else
  (void)hostname;
  (void)password;
  _debugPrint("[Vwire] Local OTA is disabled in this build");
  #endif
}

void VwireClass::handleOTA() {
  #if VWIRE_ENABLE_LOCAL_OTA
  if (_otaFeature) {
    _otaFeature->handle();
  }
  #endif
}
#endif

#if VWIRE_ENABLE_CLOUD_OTA
void VwireClass::enableCloudOTA() {
  if (_ensureOTAFeature()) {
    _otaFeature->enableCloud();
  }
}

void VwireClass::disableCloudOTA() {
  if (_otaFeature) {
    _otaFeature->disableCloud();
  }
}

bool VwireClass::isCloudOTAEnabled() {
  return _otaFeature ? _otaFeature->isCloudEnabled() : false;
}
#endif
