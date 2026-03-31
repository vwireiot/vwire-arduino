/*
 * Vwire IOT Arduino Library - Implementation
 * 
 * Professional IoT platform library for Arduino, ESP32, ESP8266 and compatible boards.
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "Vwire.h"
#include <stdarg.h>

#if VWIRE_ENABLE_LOGGING
  #define VWIRE_LOG(message) _debugPrint(message)
  #define VWIRE_LOGF(...) _debugPrintf(__VA_ARGS__)
#else
  #define VWIRE_LOG(message) do { } while (0)
  #define VWIRE_LOGF(...) do { } while (0)
#endif

// =============================================================================
// GLOBAL INSTANCE
// =============================================================================
VwireClass Vwire;
static VwireClass* _vwireInstance = nullptr;

// =============================================================================
// AUTO-REGISTRATION SYSTEM
// =============================================================================
VwireAutoHandler _vwireAutoReceiveHandlers[VWIRE_MAX_AUTO_HANDLERS];
uint8_t _vwireAutoReceiveCount = 0;
ConnectionHandler _vwireAutoConnectHandler = nullptr;
ConnectionHandler _vwireAutoDisconnectHandler = nullptr;

void _vwireRegisterReceiveHandler(uint8_t pin, PinHandler handler) {
  if (_vwireAutoReceiveCount < VWIRE_MAX_AUTO_HANDLERS) {
    _vwireAutoReceiveHandlers[_vwireAutoReceiveCount].pin = pin;
    _vwireAutoReceiveHandlers[_vwireAutoReceiveCount].handler = handler;
    _vwireAutoReceiveCount++;
  }
}

void _vwireRegisterConnectHandler(ConnectionHandler handler) {
  _vwireAutoConnectHandler = handler;
}

void _vwireRegisterDisconnectHandler(ConnectionHandler handler) {
  _vwireAutoDisconnectHandler = handler;
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================
VwireClass::VwireClass() 
  : _state(VWIRE_STATE_IDLE)
  , _lastError(VWIRE_ERR_NONE)
  , _debug(false)
  , _debugStream(&Serial)
  , _logCallback(nullptr)
  , _startTime(0)
  , _lastHeartbeat(0)
  , _lastReconnectAttempt(0)
  , _mqttClient(_wifiClient)  // Initialize with WiFiClient - CRITICAL!
  , _pinHandlerCount(0)
  , _connectHandler(nullptr)
  , _disconnectHandler(nullptr)
  , _messageHandler(nullptr)
  , _addonCount(0)
  , _gpioAddon(nullptr)
  , _reliableDeliveryAddon(nullptr)
  , _otaFeature(nullptr)
{
  memset(_deviceId, 0, sizeof(_deviceId));
  memset(_hostname, 0, sizeof(_hostname));
  memset(_pinHandlers, 0, sizeof(_pinHandlers));
  memset(_addons, 0, sizeof(_addons));
  _vwireInstance = this;
}

VwireClass::~VwireClass() {
  disconnect();
}

// =============================================================================
// CONFIGURATION
// =============================================================================
void VwireClass::config(const char* authToken, const char* deviceId,
                        VwireTransport transport) {
  // Store auth token
  strncpy(_settings.authToken, authToken, VWIRE_MAX_TOKEN_LENGTH - 1);
  _settings.authToken[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';

  // Default server
  strncpy(_settings.server, VWIRE_DEFAULT_SERVER, VWIRE_MAX_SERVER_LENGTH - 1);
  _settings.server[VWIRE_MAX_SERVER_LENGTH - 1] = '\0';

  // Transport and port
  _settings.transport = transport;
  _settings.port = (transport == VWIRE_TRANSPORT_TCP) ? VWIRE_DEFAULT_PORT_TCP : VWIRE_DEFAULT_PORT_TLS;

  // Device ID: use provided value, or fall back to auth token
  const char* id = (deviceId && strlen(deviceId) > 0) ? deviceId : authToken;
  strncpy(_deviceId, id, VWIRE_MAX_TOKEN_LENGTH - 1);
  _deviceId[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';

  VWIRE_LOGF("[Vwire] Config: device=%s, transport=%s",
               _deviceId,
               transport == VWIRE_TRANSPORT_TCP_SSL ? "TLS" : "TCP");
}

void VwireClass::setDeviceId(const char* deviceId) {
  if (deviceId && strlen(deviceId) > 0) {
    strncpy(_deviceId, deviceId, VWIRE_MAX_TOKEN_LENGTH - 1);
    _deviceId[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';
    VWIRE_LOGF("[Vwire] Custom device ID set: %s", _deviceId);
  }
}

void VwireClass::setHostname(const char* hostname) {
  if (hostname && strlen(hostname) > 0) {
    strncpy(_hostname, hostname, sizeof(_hostname) - 1);
    _hostname[sizeof(_hostname) - 1] = '\0';
    VWIRE_LOGF("[Vwire] Hostname set: %s", _hostname);
  }
}

void VwireClass::setTransport(VwireTransport transport) {
  _settings.transport = transport;
  VWIRE_LOGF("[Vwire] Transport set to: %s", 
               transport == VWIRE_TRANSPORT_TCP_SSL ? "TLS" : "TCP");
}

void VwireClass::setAutoReconnect(bool enable) {
  _settings.autoReconnect = enable;
}

void VwireClass::setReconnectInterval(unsigned long interval) {
  _settings.reconnectInterval = interval;
}

void VwireClass::setHeartbeatInterval(unsigned long interval) {
  _settings.heartbeatInterval = interval;
}

void VwireClass::setDataQoS(uint8_t qos) {
  // Note: PubSubClient library only supports QoS 0
  // This setting is kept for future compatibility if we switch to a different MQTT library
  _settings.dataQoS = (qos > 1) ? 1 : qos;
}

void VwireClass::setDataRetain(bool retain) {
  _settings.dataRetain = retain;
}

// =============================================================================
// CONNECTION
// =============================================================================
void VwireClass::_setupClient() {
  // Configure client based on transport type
  #if VWIRE_HAS_SSL
  if (_settings.transport == VWIRE_TRANSPORT_TCP_SSL) {
    #if defined(VWIRE_BOARD_ESP32)
    _secureClient.setInsecure();  // Allow self-signed certs
    _secureClient.setTimeout(10);  // 10 second timeout (reduced for faster response)
    #elif defined(VWIRE_BOARD_ESP8266)
    _secureClient.setInsecure();
    // ESP8266 BearSSL needs larger buffers for TLS
    // RX=2048 for incoming, TX=1024 for outgoing (TLS overhead needs ~500+ bytes)
    _secureClient.setBufferSizes(2048, 1024);
    _secureClient.setTimeout(10000);  // 10 second timeout (ms for ESP8266)
    #endif
    
    _mqttClient.setClient(_secureClient);
    VWIRE_LOG("[Vwire] Using TLS/SSL client");
  } else
  #endif
  {
    _mqttClient.setClient(_wifiClient);
    VWIRE_LOG("[Vwire] Using plain TCP client");
  }
  
  _mqttClient.setServer(_settings.server, _settings.port);
  _mqttClient.setCallback(_mqttCallbackWrapper);
  _mqttClient.setBufferSize(VWIRE_MAX_PAYLOAD_LENGTH);
  _mqttClient.setKeepAlive(10);       // 10 second keepalive (EMQX detects offline within ~15s)
  _mqttClient.setSocketTimeout(5);    // 5 second socket timeout (faster error detection)
}

bool VwireClass::_connectWiFi(const char* ssid, const char* password) {
  _state = VWIRE_STATE_CONNECTING_WIFI;
  VWIRE_LOGF("[Vwire] Connecting to WiFi: %s", ssid);
  
  WiFi.mode(WIFI_STA);
  
  // Set WiFi hostname for network discovery (mDNS / DHCP)
  // Priority: user-set _hostname > default "vwire-<deviceId>"
  String wifiHostname;
  if (strlen(_hostname) > 0) {
    wifiHostname = String(_hostname);
  } else {
    wifiHostname = "vwire-" + String(_deviceId).substring(0, 8);
    // Store the default so enableOTA() can reuse it
    strncpy(_hostname, wifiHostname.c_str(), sizeof(_hostname) - 1);
    _hostname[sizeof(_hostname) - 1] = '\0';
  }
  #if defined(VWIRE_BOARD_ESP32)
  WiFi.setHostname(wifiHostname.c_str());
  #elif defined(VWIRE_BOARD_ESP8266)
  WiFi.hostname(wifiHostname.c_str());
  #endif
  VWIRE_LOGF("[Vwire] WiFi hostname: %s", wifiHostname.c_str());
  
  WiFi.begin(ssid, password);
  
  unsigned long startAttempt = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
    VWIRE_LOG(".");
    
    if (millis() - startAttempt >= _settings.wifiTimeout) {
      VWIRE_LOG("\n[Vwire] WiFi connection timeout!");
      _setError(VWIRE_ERR_WIFI_FAILED);
      _state = VWIRE_STATE_ERROR;
      return false;
    }
  }
  
  VWIRE_LOGF("\n[Vwire] WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

bool VwireClass::_connectMQTT() {
  if (strlen(_settings.authToken) == 0) {
    _setError(VWIRE_ERR_NO_TOKEN);
    VWIRE_LOG("[Vwire] Error: No auth token configured!");
    return false;
  }
  
  _state = VWIRE_STATE_CONNECTING_MQTT;
  VWIRE_LOGF("[Vwire] Connecting to MQTT: %s:%d", _settings.server, _settings.port);
  
  // Generate client ID from device ID
  String clientId = "vwire-";
  clientId += _deviceId;
  
  // Last will message
  String willTopic = _buildTopic("status");
  const char* willMessage = "{\"status\":\"offline\"}";
  
  VWIRE_LOGF("[Vwire] MQTT connecting as: %s", clientId.c_str());
  
  // Connect with token as both username and password (server validates password)
  bool connected = _mqttClient.connect(clientId.c_str(), _settings.authToken, _settings.authToken, 
                          willTopic.c_str(), 1, true, willMessage);
  
  if (connected) {
    _state = VWIRE_STATE_CONNECTED;
    VWIRE_LOG("[Vwire] MQTT connected!");
    
    // Publish online status (retained so server knows device is online)
    _mqttClient.beginPublish(willTopic.c_str(), 19, true);  // retained=true
    _mqttClient.print("{\"status\":\"online\"}");
    _mqttClient.endPublish();
    
    // Subscribe to command topics with QoS 1 for reliable command delivery
    String cmdTopic = _buildTopic("cmd") + "/#";
    _mqttClient.subscribe(cmdTopic.c_str(), 1);  // QoS 1 - commands are delivered at least once
    VWIRE_LOGF("[Vwire] Subscribed to: %s (QoS 1)", cmdTopic.c_str());
    
    // Notify addons of connect (they subscribe to their own topics here)
    for (uint8_t i = 0; i < _addonCount; i++) {
      if (_addons[i]) _addons[i]->onConnect();
    }
    
    _startTime = millis();
    
    // Call connect handler (manual registration first, then auto-registered)
    if (_connectHandler) _connectHandler();
    if (_vwireAutoConnectHandler) _vwireAutoConnectHandler();
    
    return true;
  } else {
    int mqttState = _mqttClient.state();
    VWIRE_LOGF("[Vwire] MQTT failed, state=%d", mqttState);
    _setError(VWIRE_ERR_MQTT_FAILED);
    _state = VWIRE_STATE_ERROR;
    return false;
  }
}

bool VwireClass::begin(const char* ssid, const char* password) {
  VWIRE_LOG("\n[Vwire] ========================================");
  VWIRE_LOGF("[Vwire] Vwire IOT Library v%s", VWIRE_VERSION);
  VWIRE_LOGF("[Vwire] Board: %s", VWIRE_BOARD_NAME);
  VWIRE_LOG("[Vwire] ========================================\n");
  
  // Setup network client first
  _setupClient();
  
  // Connect to WiFi
  if (!_connectWiFi(ssid, password)) {
    return false;
  }
  
  // Connect to MQTT
  return _connectMQTT();
}

bool VwireClass::begin() {
  // Assume WiFi is already connected
  if (WiFi.status() != WL_CONNECTED) {
    VWIRE_LOG("[Vwire] Error: WiFi not connected!");
    _setError(VWIRE_ERR_WIFI_FAILED);
    return false;
  }
  
  _setupClient();
  return _connectMQTT();
}

void VwireClass::run() {
  // Process MQTT messages - critical for low latency command reception
  if (_mqttClient.connected()) {
    _mqttClient.loop();
    
    // Send heartbeat (only when connected)
    unsigned long now = millis();
    if (now - _lastHeartbeat >= _settings.heartbeatInterval) {
      _lastHeartbeat = now;
      _sendHeartbeat();
    }
    
    // Run addons (GPIO polling, etc.)
    for (uint8_t i = 0; i < _addonCount; i++) {
      if (_addons[i]) _addons[i]->onRun();
    }
    
    return;  // Fast path - everything is good
  }
  
  // Below here only runs when disconnected
  
  // Allow ESP8266/ESP32 network stack to process
  yield();
  
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (_state == VWIRE_STATE_CONNECTED) {
      _state = VWIRE_STATE_DISCONNECTED;
      VWIRE_LOG("[Vwire] WiFi disconnected!");
      for (uint8_t i = 0; i < _addonCount; i++) {
        if (_addons[i]) _addons[i]->onDisconnect();
      }
      if (_disconnectHandler) _disconnectHandler();
      if (_vwireAutoDisconnectHandler) _vwireAutoDisconnectHandler();
    }
    return;
  }
  
  // MQTT disconnected but WiFi is up
  if (_state == VWIRE_STATE_CONNECTED) {
    _state = VWIRE_STATE_DISCONNECTED;
    VWIRE_LOG("[Vwire] MQTT disconnected!");
    for (uint8_t i = 0; i < _addonCount; i++) {
      if (_addons[i]) _addons[i]->onDisconnect();
    }
    if (_disconnectHandler) _disconnectHandler();
    if (_vwireAutoDisconnectHandler) _vwireAutoDisconnectHandler();
  }
  
  // Attempt reconnect
  if (_settings.autoReconnect) {
    unsigned long now = millis();
    if (now - _lastReconnectAttempt >= _settings.reconnectInterval) {
      _lastReconnectAttempt = now;
      _connectMQTT();
    }
  }
}

bool VwireClass::connected() {
  return _state == VWIRE_STATE_CONNECTED && _mqttClient.connected();
}

void VwireClass::disconnect() {
  if (_mqttClient.connected()) {
    // Publish offline status (retained so server knows device went offline)
    char topic[96];
    snprintf(topic, sizeof(topic), "vwire/%s/status", _deviceId);
    _mqttClient.beginPublish(topic, 20, true);  // retained=true
    _mqttClient.print("{\"status\":\"offline\"}");
    _mqttClient.endPublish();
    _mqttClient.disconnect();
  }
  _state = VWIRE_STATE_DISCONNECTED;
}

VwireState VwireClass::getState() { return _state; }
VwireError VwireClass::getLastError() { return _lastError; }
int VwireClass::getWiFiRSSI() { return WiFi.RSSI(); }

// =============================================================================
// MQTT CALLBACK
// =============================================================================
void VwireClass::_mqttCallbackWrapper(char* topic, byte* payload, unsigned int length) {
  if (_vwireInstance) {
    _vwireInstance->_handleMessage(topic, payload, length);
  }
}

void VwireClass::_handleMessage(char* topic, byte* payload, unsigned int length) {
  // Copy payload to null-terminated string
  char payloadStr[VWIRE_MAX_PAYLOAD_LENGTH];
  int copyLen = min((unsigned int)(VWIRE_MAX_PAYLOAD_LENGTH - 1), length);
  memcpy(payloadStr, payload, copyLen);
  payloadStr[copyLen] = '\0';
  
  VWIRE_LOGF("[Vwire] Received: %s = %s", topic, payloadStr);
  
  // Call raw message handler if set
  if (_messageHandler) {
    _messageHandler(topic, payloadStr);
  }
  
  // Let addons handle the message first (GPIO pinconfig, OTA, ACK, etc.).
  // The first addon that returns true claims ownership; remaining addons and
  // the built-in virtual-pin dispatch are skipped for this message.
  for (uint8_t i = 0; i < _addonCount; i++) {
    if (_addons[i] && _addons[i]->onMessage(topic, payloadStr)) {
      return;
    }
  }
  
  // Fast parse: find "/cmd/" in topic using strstr (no heap allocation)
  char* cmdPos = strstr(topic, "/cmd/");
  if (!cmdPos) return;  // Not a command topic
  
  // Get pin string after "/cmd/"
  char* pinStr = cmdPos + 5;  // Skip "/cmd/"
  if (!pinStr || *pinStr == '\0') return;
  
  // Parse pin number (handle V prefix — virtual pins)
  int pin = -1;
  if (*pinStr == 'V' || *pinStr == 'v') {
    pin = atoi(pinStr + 1);
  } else {
    pin = atoi(pinStr);
  }
  
  // Handle the pin if valid
  if (pin >= 0 && pin < VWIRE_MAX_VIRTUAL_PINS) {
    VirtualPin vpin;
    vpin.set(payloadStr);
    
    // First, check manually registered handlers (onVirtualReceive)
    for (int i = 0; i < _pinHandlerCount; i++) {
      if (_pinHandlers[i].active && _pinHandlers[i].pin == pin) {
        if (_pinHandlers[i].handler) {
          _pinHandlers[i].handler(vpin);
        }
        return;  // Found handler, exit immediately
      }
    }
    
    // Then, check auto-registered handlers (VWIRE_RECEIVE macros)
    for (uint8_t i = 0; i < _vwireAutoReceiveCount; i++) {
      if (_vwireAutoReceiveHandlers[i].pin == pin) {
        if (_vwireAutoReceiveHandlers[i].handler) {
          _vwireAutoReceiveHandlers[i].handler(vpin);
        }
        return;  // Found handler, exit immediately
      }
    }
  }
}

// =============================================================================
// VIRTUAL PIN OPERATIONS
// =============================================================================
void VwireClass::_virtualSendInternal(uint8_t pin, const String& value) {
  if (!connected()) {
    _setError(VWIRE_ERR_NOT_CONNECTED);
    return;
  }

  if (_reliableDeliveryAddon && _reliableDeliveryAddon->send(pin, value)) {
    return;
  }
  
  // Standard fire-and-forget delivery
  // Use stack-allocated buffer for topic (avoid heap allocation)
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/pin/V%d", _deviceId, pin);
  
  // Publish data to server
  const char* payload = value.c_str();
  unsigned int len = strlen(payload);
  _mqttClient.beginPublish(topic, len, _settings.dataRetain);
  _mqttClient.print(payload);
  _mqttClient.endPublish();
  VWIRE_LOGF("[Vwire] Send V%d = %s", pin, value.c_str());
}

void VwireClass::virtualSendArray(uint8_t pin, float* values, int count) {
  String str = "";
  for (int i = 0; i < count; i++) {
    if (i > 0) str += ",";
    str += String(values[i], 2);
  }
  _virtualSendInternal(pin, str);
}

void VwireClass::virtualSendArray(uint8_t pin, int* values, int count) {
  String str = "";
  for (int i = 0; i < count; i++) {
    if (i > 0) str += ",";
    str += String(values[i]);
  }
  _virtualSendInternal(pin, str);
}

void VwireClass::virtualSendf(uint8_t pin, const char* format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  _virtualSendInternal(pin, String(buffer));
}

void VwireClass::syncVirtual(uint8_t pin) {
  if (!connected()) return;
  // Use stack buffer for topic
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/sync/V%d", _deviceId, pin);
  _mqttClient.beginPublish(topic, 0, false);
  _mqttClient.endPublish();
}

void VwireClass::syncAll() {
  if (!connected()) return;
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/sync", _deviceId);
  _mqttClient.beginPublish(topic, 3, false);
  _mqttClient.print("all");
  _mqttClient.endPublish();
}

// =============================================================================
// EVENT HANDLERS
// =============================================================================
void VwireClass::onVirtualReceive(uint8_t pin, PinHandler handler) {
  if (_pinHandlerCount >= VWIRE_MAX_HANDLERS) {
    _setError(VWIRE_ERR_HANDLER_FULL);
    VWIRE_LOG("[Vwire] Error: Max handlers reached!");
    return;
  }
  
  _pinHandlers[_pinHandlerCount].pin = pin;
  _pinHandlers[_pinHandlerCount].handler = handler;
  _pinHandlers[_pinHandlerCount].active = true;
  _pinHandlerCount++;
  
  VWIRE_LOGF("[Vwire] Handler registered for V%d", pin);
}

void VwireClass::onConnect(ConnectionHandler handler) { _connectHandler = handler; }
void VwireClass::onDisconnect(ConnectionHandler handler) { _disconnectHandler = handler; }
void VwireClass::onMessage(RawMessageHandler handler) { _messageHandler = handler; }

// =============================================================================
// DEVICE INFO
// =============================================================================
const char* VwireClass::getDeviceId() { return _deviceId; }
const char* VwireClass::getBoardName() { return VWIRE_BOARD_NAME; }
const char* VwireClass::getVersion() { return VWIRE_VERSION; }

uint32_t VwireClass::getFreeHeap() {
  #if defined(VWIRE_BOARD_ESP32) || defined(VWIRE_BOARD_ESP8266)
  return ESP.getFreeHeap();
  #else
  return 0;
  #endif
}

uint32_t VwireClass::getUptime() {
  return (millis() - _startTime) / 1000;
}

// =============================================================================
// HELPERS
// =============================================================================
String VwireClass::_buildTopic(const char* type, int pin) {
  String topic = "vwire/";
  topic += _deviceId;
  topic += "/";
  topic += type;
  
  if (pin >= 0) {
    topic += "/";
    topic += String(pin);
  }
  
  return topic;
}

void VwireClass::_sendHeartbeat() {
  if (!connected()) return;
  
  // Use stack buffers to avoid heap allocation
  char topic[96];
  char buffer[192];
  
  // Get IP address string
  String ipStr = WiFi.localIP().toString();
  
  snprintf(topic, sizeof(topic), "vwire/%s/heartbeat", _deviceId);
  
  #if VWIRE_ENABLE_CLOUD_OTA
  if (_otaFeature && _otaFeature->isCloudEnabled()) {
    snprintf(buffer, sizeof(buffer), 
      "{\"uptime\":%lu,\"heap\":%lu,\"rssi\":%d,\"ip\":\"%s\",\"fw\":\"%s\",\"ota\":true}",
      getUptime(), getFreeHeap(), getWiFiRSSI(), ipStr.c_str(), VWIRE_VERSION);
  } else {
    snprintf(buffer, sizeof(buffer), 
      "{\"uptime\":%lu,\"heap\":%lu,\"rssi\":%d,\"ip\":\"%s\",\"fw\":\"%s\"}",
      getUptime(), getFreeHeap(), getWiFiRSSI(), ipStr.c_str(), VWIRE_VERSION);
  }
  #else
  snprintf(buffer, sizeof(buffer), 
    "{\"uptime\":%lu,\"heap\":%lu,\"rssi\":%d,\"ip\":\"%s\",\"fw\":\"%s\"}",
    getUptime(), getFreeHeap(), getWiFiRSSI(), ipStr.c_str(), VWIRE_VERSION);
  #endif
  
  _mqttClient.publish(topic, buffer);
}

void VwireClass::_setError(VwireError error) {
  _lastError = error;
}

// =============================================================================
// ADDON SYSTEM
// =============================================================================

void VwireClass::addAddon(VwireAddon& addon) {
  for (uint8_t i = 0; i < _addonCount; i++) {
    if (_addons[i] == &addon) {
      return;
    }
  }

  if (_addonCount >= VWIRE_MAX_ADDONS) {
    VWIRE_LOG("[Vwire] Error: Max addons reached!");
    return;
  }

  _addons[_addonCount++] = &addon;
  addon.onAttach(*this);

  if (connected()) {
    addon.onConnect();
  }
}

// =============================================================================
// PUBLISH / SUBSCRIBE (for addons & advanced users)
// =============================================================================

bool VwireClass::publish(const char* topic, const char* payload, bool retain) {
  if (!connected()) {
    _setError(VWIRE_ERR_NOT_CONNECTED);
    return false;
  }
  unsigned int len = strlen(payload);
  _mqttClient.beginPublish(topic, len, retain);
  _mqttClient.print(payload);
  return _mqttClient.endPublish();
}

bool VwireClass::subscribe(const char* topic, uint8_t qos) {
  if (!connected()) return false;
  return _mqttClient.subscribe(topic, qos);
}

// =============================================================================
// LOG / DEBUG
// =============================================================================
void VwireClass::onLog(VwireLogCallback cb) { _logCallback = cb; }
void VwireClass::logTo(Stream& stream) {
  _debugStream = &stream;
  _debug = true;
  _logCallback = nullptr;
}
void VwireClass::disableLog() {
  _debug = false;
  _logCallback = nullptr;
}
void VwireClass::setDebug(bool enable) { _debug = enable; }
void VwireClass::setDebugStream(Stream& stream) { _debugStream = &stream; }

void VwireClass::_debugPrint(const char* message) {
  if (_logCallback) {
    _logCallback(message);
  } else if (_debug && _debugStream) {
    _debugStream->println(message);
  }
}

void VwireClass::_debugPrintf(const char* format, ...) {
  if (!_logCallback && !(_debug && _debugStream)) return;
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (_logCallback) {
    _logCallback(buffer);
  } else {
    _debugStream->println(buffer);
  }
}

void VwireClass::printDebugInfo() {
  if (!_debugStream) return;
  
  _debugStream->println(F("\n=== Vwire IOT Debug Info ==="));
  _debugStream->print(F("Version: ")); _debugStream->println(VWIRE_VERSION);
  _debugStream->print(F("Board: ")); _debugStream->println(VWIRE_BOARD_NAME);
  _debugStream->print(F("Device ID: ")); _debugStream->println(_deviceId);
  _debugStream->print(F("Server: ")); _debugStream->print(_settings.server);
  _debugStream->print(F(":")); _debugStream->println(_settings.port);
  _debugStream->print(F("Transport: ")); 
  _debugStream->println(_settings.transport == VWIRE_TRANSPORT_TCP_SSL ? "TLS" : "TCP");
  _debugStream->print(F("State: ")); _debugStream->println(_state);
  _debugStream->print(F("WiFi RSSI: ")); _debugStream->print(getWiFiRSSI()); _debugStream->println(F(" dBm"));
  _debugStream->print(F("Free Heap: ")); _debugStream->print(getFreeHeap()); _debugStream->println(F(" bytes"));
  _debugStream->print(F("Uptime: ")); _debugStream->print(getUptime()); _debugStream->println(F(" sec"));
  _debugStream->print(F("Handlers: ")); _debugStream->println(_pinHandlerCount);
  _debugStream->print(F("Reliable Delivery: "));
  _debugStream->println((_reliableDeliveryAddon && _reliableDeliveryAddon->isEnabled()) ? "ENABLED" : "DISABLED");
  if (_reliableDeliveryAddon && _reliableDeliveryAddon->isEnabled()) {
    _debugStream->print(F("Pending Messages: ")); _debugStream->println(getPendingCount());
  }
  _debugStream->println(F("============================\n"));
}
