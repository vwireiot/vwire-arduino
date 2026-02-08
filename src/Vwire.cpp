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
  , _startTime(0)
  , _lastHeartbeat(0)
  , _lastReconnectAttempt(0)
  , _mqttClient(_wifiClient)  // Initialize with WiFiClient - CRITICAL!
  , _pinHandlerCount(0)
  , _connectHandler(nullptr)
  , _disconnectHandler(nullptr)
  , _messageHandler(nullptr)
  , _deliveryCallback(nullptr)
  , _msgIdCounter(0)
  #if VWIRE_HAS_OTA
  , _otaEnabled(false)
  #endif
  #if VWIRE_ENABLE_CLOUD_OTA
  , _cloudOtaEnabled(false)
  #endif
{
  memset(_deviceId, 0, sizeof(_deviceId));
  memset(_pinHandlers, 0, sizeof(_pinHandlers));
  memset(_pendingMessages, 0, sizeof(_pendingMessages));
  _vwireInstance = this;
}

VwireClass::~VwireClass() {
  disconnect();
}

// =============================================================================
// CONFIGURATION
// =============================================================================
void VwireClass::config(const char* authToken) {
  config(authToken, VWIRE_DEFAULT_SERVER, VWIRE_DEFAULT_PORT_TLS);
}

void VwireClass::config(const char* authToken, const char* server, uint16_t port) {
  strncpy(_settings.authToken, authToken, VWIRE_MAX_TOKEN_LENGTH - 1);
  strncpy(_settings.server, server, VWIRE_MAX_SERVER_LENGTH - 1);
  _settings.port = port;
  
  // Auto-detect transport based on port
  if (port == 8883 || port == 443) {
    _settings.transport = VWIRE_TRANSPORT_TCP_SSL;
  } else {
    _settings.transport = VWIRE_TRANSPORT_TCP;
  }
  
  // Use FULL auth token as device ID for topic authorization
  strncpy(_deviceId, authToken, VWIRE_MAX_TOKEN_LENGTH - 1);
  _deviceId[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';
  
  _debugPrintf("[Vwire] Config: server=%s, port=%d, transport=%s", 
               _settings.server, _settings.port,
               _settings.transport == VWIRE_TRANSPORT_TCP_SSL ? "TLS" : "TCP");
}

void VwireClass::config(const VwireSettings& settings) {
  _settings = settings;
  
  // Use FULL auth token as device ID for topic authorization
  strncpy(_deviceId, settings.authToken, VWIRE_MAX_TOKEN_LENGTH - 1);
  _deviceId[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';
}

void VwireClass::setDeviceId(const char* deviceId) {
  if (deviceId && strlen(deviceId) > 0) {
    strncpy(_deviceId, deviceId, VWIRE_MAX_TOKEN_LENGTH - 1);
    _deviceId[VWIRE_MAX_TOKEN_LENGTH - 1] = '\0';
    _debugPrintf("[Vwire] Custom device ID set: %s", _deviceId);
  }
}

void VwireClass::setTransport(VwireTransport transport) {
  _settings.transport = transport;
  _debugPrintf("[Vwire] Transport set to: %s", 
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

void VwireClass::setReliableDelivery(bool enable) {
  _settings.reliableDelivery = enable;
  _debugPrintf("[Vwire] Reliable delivery: %s", enable ? "ENABLED" : "DISABLED");
}

void VwireClass::setAckTimeout(unsigned long timeout) {
  _settings.ackTimeout = timeout;
}

void VwireClass::setMaxRetries(uint8_t retries) {
  _settings.maxRetries = retries;
}

void VwireClass::onDeliveryStatus(DeliveryCallback cb) {
  _deliveryCallback = cb;
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
    _debugPrint("[Vwire] Using TLS/SSL client");
  } else
  #endif
  {
    _mqttClient.setClient(_wifiClient);
    _debugPrint("[Vwire] Using plain TCP client");
  }
  
  _mqttClient.setServer(_settings.server, _settings.port);
  _mqttClient.setCallback(_mqttCallbackWrapper);
  _mqttClient.setBufferSize(VWIRE_MAX_PAYLOAD_LENGTH);
  _mqttClient.setKeepAlive(30);       // 30 second keepalive (faster disconnect detection)
  _mqttClient.setSocketTimeout(5);    // 5 second socket timeout (faster error detection)
}

bool VwireClass::_connectWiFi(const char* ssid, const char* password) {
  _state = VWIRE_STATE_CONNECTING_WIFI;
  _debugPrintf("[Vwire] Connecting to WiFi: %s", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unsigned long startAttempt = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
    _debugPrint(".");
    
    if (millis() - startAttempt >= _settings.wifiTimeout) {
      _debugPrint("\n[Vwire] WiFi connection timeout!");
      _setError(VWIRE_ERR_WIFI_FAILED);
      _state = VWIRE_STATE_ERROR;
      return false;
    }
  }
  
  _debugPrintf("\n[Vwire] WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

bool VwireClass::_connectMQTT() {
  if (strlen(_settings.authToken) == 0) {
    _setError(VWIRE_ERR_NO_TOKEN);
    _debugPrint("[Vwire] Error: No auth token configured!");
    return false;
  }
  
  _state = VWIRE_STATE_CONNECTING_MQTT;
  _debugPrintf("[Vwire] Connecting to MQTT: %s:%d", _settings.server, _settings.port);
  
  // Generate client ID from device ID
  String clientId = "vwire-";
  clientId += _deviceId;
  
  // Last will message
  String willTopic = _buildTopic("status");
  const char* willMessage = "{\"status\":\"offline\"}";
  
  _debugPrintf("[Vwire] MQTT connecting as: %s", clientId.c_str());
  
  // Connect with token as both username and password (server validates password)
  bool connected = _mqttClient.connect(clientId.c_str(), _settings.authToken, _settings.authToken, 
                          willTopic.c_str(), 1, true, willMessage);
  
  if (connected) {
    _state = VWIRE_STATE_CONNECTED;
    _debugPrint("[Vwire] MQTT connected!");
    
    // Publish online status (retained so server knows device is online)
    _mqttClient.beginPublish(willTopic.c_str(), 19, true);  // retained=true
    _mqttClient.print("{\"status\":\"online\"}");
    _mqttClient.endPublish();
    
    // Subscribe to command topics with QoS 1 for reliable command delivery
    String cmdTopic = _buildTopic("cmd") + "/#";
    _mqttClient.subscribe(cmdTopic.c_str(), 1);  // QoS 1 - commands are delivered at least once
    _debugPrintf("[Vwire] Subscribed to: %s (QoS 1)", cmdTopic.c_str());
    
    // Subscribe to ACK topic for reliable delivery (if enabled)
    if (_settings.reliableDelivery) {
      String ackTopic = _buildTopic("ack");
      _mqttClient.subscribe(ackTopic.c_str(), 1);
      _debugPrintf("[Vwire] Subscribed to: %s (ACK)", ackTopic.c_str());
    }
    
    // Subscribe to Cloud OTA topic (if enabled)
    #if VWIRE_ENABLE_CLOUD_OTA
    if (_cloudOtaEnabled) {
      String otaTopic = _buildTopic("ota");
      _mqttClient.subscribe(otaTopic.c_str(), 1);
      _debugPrintf("[Vwire] Subscribed to: %s (Cloud OTA)", otaTopic.c_str());
    }
    #endif
    
    _startTime = millis();
    
    // Call connect handler (manual registration first, then auto-registered)
    if (_connectHandler) _connectHandler();
    if (_vwireAutoConnectHandler) _vwireAutoConnectHandler();
    
    return true;
  } else {
    int mqttState = _mqttClient.state();
    _debugPrintf("[Vwire] MQTT failed, state=%d", mqttState);
    _setError(VWIRE_ERR_MQTT_FAILED);
    _state = VWIRE_STATE_ERROR;
    return false;
  }
}

bool VwireClass::begin(const char* ssid, const char* password) {
  _debugPrint("\n[Vwire] ========================================");
  _debugPrintf("[Vwire] Vwire IOT Library v%s", VWIRE_VERSION);
  _debugPrintf("[Vwire] Board: %s", VWIRE_BOARD_NAME);
  _debugPrint("[Vwire] ========================================\n");
  
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
    _debugPrint("[Vwire] Error: WiFi not connected!");
    _setError(VWIRE_ERR_WIFI_FAILED);
    return false;
  }
  
  _setupClient();
  return _connectMQTT();
}

void VwireClass::run() {
  // Process MQTT messages FIRST - critical for low latency command reception
  if (_mqttClient.connected()) {
    _mqttClient.loop();
    
    // Process reliable delivery retries (if enabled)
    if (_settings.reliableDelivery) {
      _processRetries();
    }
    
    // Send heartbeat (only when connected)
    unsigned long now = millis();
    if (now - _lastHeartbeat >= _settings.heartbeatInterval) {
      _lastHeartbeat = now;
      _sendHeartbeat();
    }
    return;  // Fast path - everything is good
  }
  
  // Below here only runs when disconnected
  
  // Allow ESP8266/ESP32 network stack to process
  yield();
  
  // Handle OTA if enabled
  #if VWIRE_HAS_OTA
  if (_otaEnabled) {
    ArduinoOTA.handle();
  }
  #endif
  
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (_state == VWIRE_STATE_CONNECTED) {
      _state = VWIRE_STATE_DISCONNECTED;
      _debugPrint("[Vwire] WiFi disconnected!");
      if (_disconnectHandler) _disconnectHandler();
      if (_vwireAutoDisconnectHandler) _vwireAutoDisconnectHandler();
    }
    return;
  }
  
  // MQTT disconnected but WiFi is up
  if (_state == VWIRE_STATE_CONNECTED) {
    _state = VWIRE_STATE_DISCONNECTED;
    _debugPrint("[Vwire] MQTT disconnected!");
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
  
  _debugPrintf("[Vwire] Received: %s = %s", topic, payloadStr);
  
  // Call raw message handler if set
  if (_messageHandler) {
    _messageHandler(topic, payloadStr);
  }
  
  // Check for Cloud OTA topic: vwire/{deviceId}/ota
  #if VWIRE_ENABLE_CLOUD_OTA
  if (_cloudOtaEnabled) {
    char* otaPos = strstr(topic, "/ota");
    if (otaPos && *(otaPos + 4) == '\0') {
      _handleCloudOTA(payloadStr);
      return;
    }
  }
  #endif
  
  // Check for ACK topic first: vwire/{deviceId}/ack
  char* ackPos = strstr(topic, "/ack");
  if (ackPos && *(ackPos + 4) == '\0') {
    // This is an ACK message - parse JSON: {"msgId":"xxx","ok":true/false}
    // Simple parse without ArduinoJson to save memory
    char* msgIdStart = strstr(payloadStr, "\"msgId\":\"");
    char* okStart = strstr(payloadStr, "\"ok\":");
    
    if (msgIdStart && okStart) {
      msgIdStart += 9;  // Skip to value
      char* msgIdEnd = strchr(msgIdStart, '\"');
      if (msgIdEnd) {
        char msgId[16];
        int len = min((int)(msgIdEnd - msgIdStart), 15);
        strncpy(msgId, msgIdStart, len);
        msgId[len] = '\0';
        
        bool success = (strstr(okStart, "true") != nullptr);
        _handleAck(msgId, success);
      }
    }
    return;  // ACK processed, don't continue as command
  }
  
  // Fast parse: find "/cmd/" in topic using strstr (no heap allocation)
  char* cmdPos = strstr(topic, "/cmd/");
  if (!cmdPos) return;  // Not a command topic
  
  // Get pin string after "/cmd/"
  char* pinStr = cmdPos + 5;  // Skip "/cmd/"
  if (!pinStr || *pinStr == '\0') return;
  
  // Parse pin number (handle V prefix)
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
  
  // Use reliable delivery if enabled
  if (_settings.reliableDelivery) {
    _sendWithReliableDelivery(pin, value);
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
  _debugPrintf("[Vwire] Send V%d = %s", pin, value.c_str());
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
    _debugPrint("[Vwire] Error: Max handlers reached!");
    return;
  }
  
  _pinHandlers[_pinHandlerCount].pin = pin;
  _pinHandlers[_pinHandlerCount].handler = handler;
  _pinHandlers[_pinHandlerCount].active = true;
  _pinHandlerCount++;
  
  _debugPrintf("[Vwire] Handler registered for V%d", pin);
}

void VwireClass::onConnect(ConnectionHandler handler) { _connectHandler = handler; }
void VwireClass::onDisconnect(ConnectionHandler handler) { _disconnectHandler = handler; }
void VwireClass::onMessage(RawMessageHandler handler) { _messageHandler = handler; }

// =============================================================================
// NOTIFICATIONS
// =============================================================================
void VwireClass::notify(const char* message) {
  if (!connected()) return;
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/notify", _deviceId);
  unsigned int len = strlen(message);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(message);
  _mqttClient.endPublish();
  _debugPrintf("[Vwire] Notify: %s", message);
}

void VwireClass::alarm(const char* message) {
  alarm(message, "default", 1);
}

void VwireClass::alarm(const char* message, const char* sound) {
  alarm(message, sound, 1);
}

void VwireClass::alarm(const char* message, const char* sound, uint8_t priority) {
  if (!connected()) return;
  
  // Generate unique alarm ID to prevent duplicates
  static unsigned long lastAlarmId = 0;
  unsigned long alarmId = millis();
  if (alarmId == lastAlarmId) alarmId++; // Ensure uniqueness
  lastAlarmId = alarmId;
  
  char topic[96];
  char buffer[VWIRE_JSON_BUFFER_SIZE];
  
  snprintf(topic, sizeof(topic), "vwire/%s/alarm", _deviceId);
  snprintf(buffer, sizeof(buffer), 
    "{\"type\":\"alarm\",\"message\":\"%s\",\"alarmId\":\"alarm_%lu\",\"sound\":\"%s\",\"priority\":%d,\"timestamp\":%lu}", 
    message, alarmId, sound, priority, millis()
  );
  
  unsigned int len = strlen(buffer);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(buffer);
  _mqttClient.endPublish();
  _debugPrintf("[Vwire] Alarm: %s (sound: %s, priority: %d)", message, sound, priority);
}

void VwireClass::email(const char* subject, const char* body) {
  if (!connected()) return;
  
  char topic[96];
  char buffer[VWIRE_JSON_BUFFER_SIZE];
  
  snprintf(topic, sizeof(topic), "vwire/%s/email", _deviceId);
  snprintf(buffer, sizeof(buffer), "{\"subject\":\"%s\",\"body\":\"%s\"}", subject, body);
  
  unsigned int len = strlen(buffer);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(buffer);
  _mqttClient.endPublish();
  _debugPrintf("[Vwire] Email: %s", subject);
}

void VwireClass::log(const char* message) {
  if (!connected()) return;
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/log", _deviceId);
  unsigned int len = strlen(message);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(message);
  _mqttClient.endPublish();
}

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
// OTA
// =============================================================================
#if VWIRE_HAS_OTA
void VwireClass::enableOTA(const char* hostname, const char* password) {
  if (hostname) {
    ArduinoOTA.setHostname(hostname);
  } else {
    String defaultHostname = "vwire-" + String(_deviceId).substring(0, 8);
    ArduinoOTA.setHostname(defaultHostname.c_str());
  }
  
  if (password) {
    ArduinoOTA.setPassword(password);
  }
  
  ArduinoOTA.onStart([this]() {
    _debugPrint("[Vwire] OTA Update starting...");
  });
  
  ArduinoOTA.onEnd([this]() {
    _debugPrint("[Vwire] OTA Update complete!");
  });
  
  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    _debugPrintf("[Vwire] OTA Progress: %u%%", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([this](ota_error_t error) {
    _debugPrintf("[Vwire] OTA Error[%u]", error);
  });
  
  ArduinoOTA.begin();
  _otaEnabled = true;
  _debugPrint("[Vwire] OTA enabled");
}

void VwireClass::handleOTA() {
  if (_otaEnabled) ArduinoOTA.handle();
}
#endif

// =============================================================================
// CLOUD OTA (Firmware update from VWire server)
// =============================================================================
#if VWIRE_ENABLE_CLOUD_OTA

void VwireClass::enableCloudOTA() {
  _cloudOtaEnabled = true;
  _debugPrint("[Vwire] Cloud OTA enabled");
  
  // If already connected, subscribe to OTA topic immediately
  if (connected()) {
    String otaTopic = _buildTopic("ota");
    _mqttClient.subscribe(otaTopic.c_str(), 1);
    _debugPrintf("[Vwire] Subscribed to: %s (Cloud OTA)", otaTopic.c_str());
  }
}

bool VwireClass::isCloudOTAEnabled() {
  return _cloudOtaEnabled;
}

void VwireClass::_ensureMqttForOTA() {
  if (_mqttClient.connected()) return;
  
  _debugPrint("[Vwire] MQTT disconnected during OTA download, reconnecting...");
  
  // Quick reconnect attempts (3 tries, 1 second apart)
  for (int i = 0; i < 3; i++) {
    _setupClient();
    if (_connectMQTT()) {
      _debugPrint("[Vwire] MQTT reconnected for OTA status report");
      return;
    }
    delay(1000);
  }
  
  _debugPrint("[Vwire] MQTT reconnect failed - OTA status may not be reported");
}

void VwireClass::_publishOTAStatus(const char* updateId, const char* status, int progress, const char* error) {
  if (!connected()) return;
  
  char topic[96];
  char buffer[256];
  
  snprintf(topic, sizeof(topic), "vwire/%s/ota_status", _deviceId);
  
  if (error) {
    snprintf(buffer, sizeof(buffer),
      "{\"updateId\":\"%s\",\"status\":\"%s\",\"progress\":%d,\"error\":\"%s\",\"version\":\"%s\"}",
      updateId, status, progress, error, VWIRE_VERSION);
  } else {
    snprintf(buffer, sizeof(buffer),
      "{\"updateId\":\"%s\",\"status\":\"%s\",\"progress\":%d,\"version\":\"%s\"}",
      updateId, status, progress, VWIRE_VERSION);
  }
  
  _mqttClient.publish(topic, buffer, true);  // retained so server gets it
  _debugPrintf("[Vwire] OTA Status: %s %d%%", status, progress);
}

void VwireClass::_handleCloudOTA(const char* payload) {
  _debugPrint("[Vwire] Cloud OTA command received");
  
  // Parse JSON payload: {url, version, checksum, size, updateId}
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload);
  
  if (err) {
    _debugPrintf("[Vwire] OTA JSON parse error: %s", err.c_str());
    return;
  }
  
  const char* url = doc["url"];
  const char* version = doc["version"];
  const char* checksum = doc["checksum"];
  int size = doc["size"] | 0;
  const char* updateId = doc["updateId"];
  
  if (!url || !updateId) {
    _debugPrint("[Vwire] OTA command missing required fields");
    return;
  }
  
  _debugPrintf("[Vwire] OTA: url=%s", url);
  _debugPrintf("[Vwire] OTA: version=%s size=%d", version ? version : "?", size);
  
  // Report downloading status
  _publishOTAStatus(updateId, "downloading", 0);
  
  // Give MQTT time to send the status message
  _mqttClient.loop();
  delay(100);
  
  // Determine if HTTPS is needed
  bool useHttps = (strncmp(url, "https", 5) == 0);
  
  // Create appropriate client for HTTP or HTTPS
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  if (useHttps) {
    secureClient.setInsecure();  // Skip certificate verification (firmware has its own checksum)
    _debugPrint("[Vwire] OTA: Using HTTPS for firmware download");
  }
  WiFiClient& updateClient = useHttps ? (WiFiClient&)secureClient : plainClient;
  
  #if defined(VWIRE_BOARD_ESP32)
  // ESP32: Use HTTPUpdate
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  httpUpdate.rebootOnUpdate(false);  // Don't auto-reboot, we want to send status first
  
  t_httpUpdate_return ret = httpUpdate.update(updateClient, url);
  
  // MQTT likely disconnected during the blocking download (keep-alive timeout).
  // Reconnect so we can publish the OTA result status before rebooting.
  _ensureMqttForOTA();
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      _debugPrintf("[Vwire] OTA FAILED: (%d) %s", 
        httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      _publishOTAStatus(updateId, "failed", 0, httpUpdate.getLastErrorString().c_str());
      _mqttClient.loop();
      delay(200);
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      _debugPrint("[Vwire] OTA: No update available");
      _publishOTAStatus(updateId, "failed", 0, "No update available");
      _mqttClient.loop();
      delay(200);
      break;
      
    case HTTP_UPDATE_OK:
      _debugPrint("[Vwire] OTA SUCCESS - Rebooting...");
      _publishOTAStatus(updateId, "completed", 100);
      _mqttClient.loop();
      delay(1000);  // Extra time to ensure MQTT message is sent
      ESP.restart();
      break;
  }
  
  #elif defined(VWIRE_BOARD_ESP8266)
  // ESP8266: Use ESPhttpUpdate
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
  ESPhttpUpdate.rebootOnUpdate(false);
  
  t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, url);
  
  // MQTT likely disconnected during the blocking download (keep-alive timeout).
  // Reconnect so we can publish the OTA result status before rebooting.
  _ensureMqttForOTA();
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      _debugPrintf("[Vwire] OTA FAILED: (%d) %s",
        ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      _publishOTAStatus(updateId, "failed", 0, ESPhttpUpdate.getLastErrorString().c_str());
      _mqttClient.loop();
      delay(200);
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      _debugPrint("[Vwire] OTA: No update available");
      _publishOTAStatus(updateId, "failed", 0, "No update available");
      _mqttClient.loop();
      delay(200);
      break;
      
    case HTTP_UPDATE_OK:
      _debugPrint("[Vwire] OTA SUCCESS - Rebooting...");
      _publishOTAStatus(updateId, "completed", 100);
      _mqttClient.loop();
      delay(1000);  // Extra time to ensure MQTT message is sent
      ESP.restart();
      break;
  }
  #endif
}

#endif // VWIRE_ENABLE_CLOUD_OTA

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
  if (_cloudOtaEnabled) {
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
// DEBUG
// =============================================================================
void VwireClass::setDebug(bool enable) { _debug = enable; }
void VwireClass::setDebugStream(Stream& stream) { _debugStream = &stream; }

void VwireClass::_debugPrint(const char* message) {
  if (_debug && _debugStream) {
    _debugStream->println(message);
  }
}

void VwireClass::_debugPrintf(const char* format, ...) {
  if (_debug && _debugStream) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
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
  _debugStream->print(F("Reliable Delivery: ")); _debugStream->println(_settings.reliableDelivery ? "ENABLED" : "DISABLED");
  if (_settings.reliableDelivery) {
    _debugStream->print(F("Pending Messages: ")); _debugStream->println(getPendingCount());
  }
  _debugStream->println(F("============================\n"));
}

// =============================================================================
// RELIABLE DELIVERY
// =============================================================================
uint8_t VwireClass::getPendingCount() {
  uint8_t count = 0;
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (_pendingMessages[i].active) count++;
  }
  return count;
}

bool VwireClass::isDeliveryPending() {
  return getPendingCount() > 0;
}

int VwireClass::_findPendingSlot() {
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (!_pendingMessages[i].active) return i;
  }
  return -1;  // Queue full
}

void VwireClass::_removePending(const char* msgId) {
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (_pendingMessages[i].active && strcmp(_pendingMessages[i].msgId, msgId) == 0) {
      _pendingMessages[i].active = false;
      _debugPrintf("[Vwire] Removed pending message: %s", msgId);
      return;
    }
  }
}

void VwireClass::_handleAck(const char* msgId, bool success) {
  _debugPrintf("[Vwire] ACK received: %s = %s", msgId, success ? "OK" : "FAIL");
  
  // Find the pending message
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (_pendingMessages[i].active && strcmp(_pendingMessages[i].msgId, msgId) == 0) {
      _pendingMessages[i].active = false;
      
      // Call delivery callback if set
      if (_deliveryCallback) {
        _deliveryCallback(msgId, success);
      }
      
      if (success) {
        _debugPrintf("[Vwire] ✓ Message %s delivered successfully", msgId);
      } else {
        _debugPrintf("[Vwire] ✗ Message %s delivery failed (server NACK)", msgId);
      }
      return;
    }
  }
  
  // Message not found in pending queue (might be duplicate ACK)
  _debugPrintf("[Vwire] ACK for unknown message: %s (possibly duplicate)", msgId);
}

void VwireClass::_sendWithReliableDelivery(uint8_t pin, const String& value) {
  // Find empty slot in pending queue
  int slot = _findPendingSlot();
  if (slot < 0) {
    _setError(VWIRE_ERR_QUEUE_FULL);
    _debugPrint("[Vwire] Error: Reliable delivery queue full!");
    
    // Notify callback of failure
    if (_deliveryCallback) {
      _deliveryCallback("queue_full", false);
    }
    return;
  }
  
  // Generate unique message ID: deviceId_counter_millis
  _msgIdCounter++;
  snprintf(_pendingMessages[slot].msgId, sizeof(_pendingMessages[slot].msgId),
           "%04X_%lu", (uint16_t)(_msgIdCounter & 0xFFFF), millis() % 10000);
  
  _pendingMessages[slot].pin = pin;
  strncpy(_pendingMessages[slot].value, value.c_str(), sizeof(_pendingMessages[slot].value) - 1);
  _pendingMessages[slot].value[sizeof(_pendingMessages[slot].value) - 1] = '\0';
  _pendingMessages[slot].sentAt = millis();
  _pendingMessages[slot].retries = 0;
  _pendingMessages[slot].active = true;
  
  // Build payload with msgId: {"msgId":"xxx","pin":"V0","value":"123"}
  char payload[VWIRE_JSON_BUFFER_SIZE];
  snprintf(payload, sizeof(payload), 
           "{\"msgId\":\"%s\",\"pin\":\"V%d\",\"value\":\"%s\"}",
           _pendingMessages[slot].msgId, pin, _pendingMessages[slot].value);
  
  // Use /data topic for reliable messages (server will ACK these)
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/data", _deviceId);
  
  unsigned int len = strlen(payload);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(payload);
  _mqttClient.endPublish();
  
  _debugPrintf("[Vwire] Reliable write V%d = %s (msgId: %s)", 
               pin, value.c_str(), _pendingMessages[slot].msgId);
}

void VwireClass::_processRetries() {
  unsigned long now = millis();
  
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (!_pendingMessages[i].active) continue;
    
    // Check if ACK timeout has passed
    if (now - _pendingMessages[i].sentAt >= _settings.ackTimeout) {
      if (_pendingMessages[i].retries < _settings.maxRetries) {
        // Retry
        _pendingMessages[i].retries++;
        _pendingMessages[i].sentAt = now;
        
        // Rebuild and resend payload
        char payload[VWIRE_JSON_BUFFER_SIZE];
        snprintf(payload, sizeof(payload), 
                 "{\"msgId\":\"%s\",\"pin\":\"V%d\",\"value\":\"%s\"}",
                 _pendingMessages[i].msgId, _pendingMessages[i].pin, _pendingMessages[i].value);
        
        char topic[96];
        snprintf(topic, sizeof(topic), "vwire/%s/data", _deviceId);
        
        unsigned int len = strlen(payload);
        _mqttClient.beginPublish(topic, len, false);
        _mqttClient.print(payload);
        _mqttClient.endPublish();
        
        _debugPrintf("[Vwire] ↻ Retry %d/%d for message %s", 
                     _pendingMessages[i].retries, _settings.maxRetries,
                     _pendingMessages[i].msgId);
      } else {
        // Max retries exceeded - give up
        _debugPrintf("[Vwire] ✗ Message %s dropped after %d retries", 
                     _pendingMessages[i].msgId, _settings.maxRetries);
        
        // Call delivery callback with failure
        if (_deliveryCallback) {
          _deliveryCallback(_pendingMessages[i].msgId, false);
        }
        
        _pendingMessages[i].active = false;
      }
    }
  }
}