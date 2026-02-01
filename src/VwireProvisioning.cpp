/*
 * Vwire IOT Arduino Library - WiFi Provisioning Implementation
 *
 * Implements AP Mode provisioning for ESP32/ESP8266.
 *
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireProvisioning.h"
#include <stdarg.h>
#include <ArduinoJson.h>

// =============================================================================
// GLOBAL INSTANCE
// =============================================================================
VwireProvisioningClass VwireProvision;

#if VWIRE_HAS_AP_PROVISIONING
VwireProvisioningClass* VwireProvisioningClass::_instance = nullptr;
#endif

// =============================================================================
// HTML TEMPLATES FOR AP MODE
// =============================================================================
#if VWIRE_HAS_AP_PROVISIONING

static const char PROGMEM HTML_HEADER[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VWire Device Setup</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
      min-height: 100vh;
      color: #fff;
      padding: 20px;
    }
    .container {
      max-width: 400px;
      margin: 0 auto;
      background: rgba(255,255,255,0.05);
      border-radius: 16px;
      padding: 24px;
      backdrop-filter: blur(10px);
    }
    .logo {
      text-align: center;
      margin-bottom: 24px;
    }
    .logo h1 {
      font-size: 24px;
      font-weight: 600;
      color: #00d4ff;
    }
    .logo p {
      color: #888;
      font-size: 14px;
      margin-top: 4px;
    }
    .form-group {
      margin-bottom: 16px;
    }
    label {
      display: block;
      margin-bottom: 6px;
      font-size: 14px;
      color: #aaa;
    }
    input {
      width: 100%;
      padding: 12px 16px;
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 8px;
      background: rgba(0,0,0,0.3);
      color: #fff;
      font-size: 16px;
      outline: none;
      transition: border-color 0.2s;
    }
    input:focus {
      border-color: #00d4ff;
    }
    input::placeholder {
      color: #666;
    }
    button {
      width: 100%;
      padding: 14px;
      border: none;
      border-radius: 8px;
      background: #00d4ff;
      color: #000;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      margin-top: 8px;
      transition: opacity 0.2s;
    }
    button:hover {
      opacity: 0.9;
    }
    button:disabled {
      background: #444;
      color: #888;
      cursor: not-allowed;
    }
    .status {
      margin-top: 16px;
      padding: 12px;
      border-radius: 8px;
      text-align: center;
      font-size: 14px;
    }
    .status.success { background: rgba(0,200,100,0.2); color: #0c6; }
    .status.error { background: rgba(255,100,100,0.2); color: #f66; }
    .status.info { background: rgba(0,200,255,0.2); color: #0cf; }
    .note {
      margin-top: 16px;
      padding: 12px;
      background: rgba(255,200,0,0.1);
      border-radius: 8px;
      font-size: 12px;
      color: #aa8;
    }
    .spinner {
      display: inline-block;
      width: 16px;
      height: 16px;
      border: 2px solid #fff;
      border-top-color: transparent;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin-right: 8px;
      vertical-align: middle;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
  </style>
</head>
<body>
  <div class="container">
)rawliteral";

static const char PROGMEM HTML_FOOTER[] = R"rawliteral(
  </div>
</body>
</html>
)rawliteral";

static const char PROGMEM HTML_CONFIG_FORM[] = R"rawliteral(
    <div class="logo">
      <h1>VWire Setup</h1>
      <p>Configure your IoT device</p>
    </div>
    
    <form id="configForm" onsubmit="return submitConfig()">
      <div class="form-group">
        <label for="ssid">WiFi Network (SSID)</label>
        <input type="text" id="ssid" name="ssid" placeholder="Your WiFi name" required maxlength="32">
      </div>
      
      <div class="form-group">
        <label for="password">WiFi Password</label>
        <input type="password" id="password" name="password" placeholder="WiFi password" maxlength="64">
      </div>
      
      <div class="form-group">
        <label for="token">Device Token</label>
        <input type="text" id="token" name="token" placeholder="From VWire dashboard" required maxlength="63">
      </div>
      
      <button type="submit" id="submitBtn">Configure Device</button>
      
      <div id="status" class="status" style="display:none;"></div>
      
      <div class="note">
        <strong>Note:</strong> After configuration, the device will restart and connect to your WiFi network.
        You can then close this page.
      </div>
    </form>
    
    <script>
      function submitConfig() {
        var btn = document.getElementById('submitBtn');
        var status = document.getElementById('status');
        var ssid = document.getElementById('ssid').value;
        var password = document.getElementById('password').value;
        var token = document.getElementById('token').value;
        
        btn.disabled = true;
        btn.innerHTML = '<span class="spinner"></span>Configuring...';
        status.style.display = 'none';
        
        fetch('/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'ssid=' + encodeURIComponent(ssid) + 
                '&password=' + encodeURIComponent(password) + 
                '&token=' + encodeURIComponent(token)
        })
        .then(function(response) { return response.json(); })
        .then(function(data) {
          if (data.success) {
            status.className = 'status success';
            status.innerHTML = '✓ Configuration saved! Device is restarting...';
            status.style.display = 'block';
          } else {
            throw new Error(data.error || 'Configuration failed');
          }
        })
        .catch(function(error) {
          status.className = 'status error';
          status.innerHTML = '✗ ' + error.message;
          status.style.display = 'block';
          btn.disabled = false;
          btn.innerHTML = 'Configure Device';
        });
        
        return false;
      }
    </script>
)rawliteral";

#endif // VWIRE_HAS_AP_PROVISIONING

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================
VwireProvisioningClass::VwireProvisioningClass()
  : _state(VWIRE_PROV_IDLE)
  , _method(VWIRE_PROV_METHOD_NONE)
  , _credentialsLoaded(false)
  , _startTime(0)
  , _timeout(0)
  , _stateCallback(nullptr)
  , _credentialsCallback(nullptr)
  , _progressCallback(nullptr)
  , _debug(false)
  , _debugStream(&Serial)
  #if VWIRE_HAS_AP_PROVISIONING
  , _webServer(nullptr)
  , _pendingStopAP(false)
  , _pendingConnect(false)
  #endif
{
  memset(&_credentials, 0, sizeof(_credentials));
  #if VWIRE_HAS_AP_PROVISIONING
  memset(_apSSID, 0, sizeof(_apSSID));
  _instance = this;
  #endif
}

VwireProvisioningClass::~VwireProvisioningClass() {
  stop();
  #if VWIRE_HAS_AP_PROVISIONING
  if (_webServer) {
    delete _webServer;
    _webServer = nullptr;
  }
  #endif
}

// =============================================================================
// CREDENTIALS MANAGEMENT
// =============================================================================
bool VwireProvisioningClass::hasCredentials() {
  if (!_credentialsLoaded) {
    loadCredentials();
  }
  return _credentials.isValid() && strlen(_credentials.ssid) > 0;
}

bool VwireProvisioningClass::loadCredentials() {
  _credentialsLoaded = true;
  return _loadFromStorage();
}

bool VwireProvisioningClass::saveCredentials(const char* ssid, const char* password, const char* authToken) {
  // Clear and set magic
  memset(&_credentials, 0, sizeof(_credentials));
  _credentials.magic = VWIRE_PROV_MAGIC;
  
  // Copy credentials
  strncpy(_credentials.ssid, ssid, VWIRE_PROV_MAX_SSID_LEN - 1);
  strncpy(_credentials.password, password, VWIRE_PROV_MAX_PASS_LEN - 1);
  strncpy(_credentials.authToken, authToken, VWIRE_PROV_MAX_TOKEN_LEN - 1);
  
  // Calculate checksum
  _credentials.checksum = _credentials.calcChecksum();
  
  _credentialsLoaded = true;
  
  return _saveToStorage();
}

bool VwireProvisioningClass::clearCredentials() {
  memset(&_credentials, 0, sizeof(_credentials));
  _credentialsLoaded = true;
  return _clearStorage();
}

const char* VwireProvisioningClass::getSSID() {
  if (!_credentialsLoaded) loadCredentials();
  return _credentials.ssid;
}

const char* VwireProvisioningClass::getPassword() {
  if (!_credentialsLoaded) loadCredentials();
  return _credentials.password;
}

const char* VwireProvisioningClass::getAuthToken() {
  if (!_credentialsLoaded) loadCredentials();
  return _credentials.authToken;
}

// =============================================================================
// STORAGE IMPLEMENTATION
// =============================================================================

#if defined(VWIRE_BOARD_ESP32)
// ESP32 uses Preferences library (NVS)
bool VwireProvisioningClass::_loadFromStorage() {
  _preferences.begin(VWIRE_PROV_NAMESPACE, true);  // Read-only
  
  size_t len = _preferences.getBytes("cred", &_credentials, sizeof(_credentials));
  
  _preferences.end();
  
  if (len != sizeof(_credentials)) {
    _debugPrint("[Provision] No stored credentials found");
    return false;
  }
  
  if (!_credentials.isValid()) {
    _debugPrint("[Provision] Stored credentials invalid (checksum mismatch)");
    return false;
  }
  
  _debugPrintf("[Provision] Loaded credentials for SSID: %s", _credentials.ssid);
  return true;
}

bool VwireProvisioningClass::_saveToStorage() {
  _preferences.begin(VWIRE_PROV_NAMESPACE, false);  // Read-write
  
  size_t written = _preferences.putBytes("cred", &_credentials, sizeof(_credentials));
  
  _preferences.end();
  
  if (written != sizeof(_credentials)) {
    _debugPrint("[Provision] Failed to save credentials!");
    return false;
  }
  
  _debugPrintf("[Provision] Saved credentials for SSID: %s", _credentials.ssid);
  return true;
}

bool VwireProvisioningClass::_clearStorage() {
  _preferences.begin(VWIRE_PROV_NAMESPACE, false);
  _preferences.clear();
  _preferences.end();
  _debugPrint("[Provision] Credentials cleared");
  return true;
}

#elif defined(VWIRE_BOARD_ESP8266)
// ESP8266 uses EEPROM
bool VwireProvisioningClass::_loadFromStorage() {
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  uint8_t* ptr = (uint8_t*)&_credentials;
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    ptr[i] = EEPROM.read(VWIRE_PROV_EEPROM_START + i);
  }
  
  EEPROM.end();
  
  if (!_credentials.isValid()) {
    _debugPrint("[Provision] No valid credentials in EEPROM");
    return false;
  }
  
  _debugPrintf("[Provision] Loaded credentials for SSID: %s", _credentials.ssid);
  return true;
}

bool VwireProvisioningClass::_saveToStorage() {
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  uint8_t* ptr = (uint8_t*)&_credentials;
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    EEPROM.write(VWIRE_PROV_EEPROM_START + i, ptr[i]);
  }
  
  bool result = EEPROM.commit();
  EEPROM.end();
  
  if (!result) {
    _debugPrint("[Provision] Failed to save to EEPROM!");
    return false;
  }
  
  _debugPrintf("[Provision] Saved credentials for SSID: %s", _credentials.ssid);
  return true;
}

bool VwireProvisioningClass::_clearStorage() {
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    EEPROM.write(VWIRE_PROV_EEPROM_START + i, 0);
  }
  
  bool result = EEPROM.commit();
  EEPROM.end();
  
  _debugPrint("[Provision] Credentials cleared from EEPROM");
  return result;
}

#else
// Unsupported platform - no persistent storage
bool VwireProvisioningClass::_loadFromStorage() { return false; }
bool VwireProvisioningClass::_saveToStorage() { return false; }
bool VwireProvisioningClass::_clearStorage() { return true; }
#endif

// =============================================================================
// AP MODE PROVISIONING
// =============================================================================

// =============================================================================
// AP MODE PROVISIONING
// =============================================================================
#if VWIRE_HAS_AP_PROVISIONING

bool VwireProvisioningClass::startAPMode(const char* apPassword, unsigned long timeout) {
  // Generate unique AP SSID using chip ID
  #if defined(VWIRE_BOARD_ESP32)
  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() >> 32);
  #elif defined(VWIRE_BOARD_ESP8266)
  uint32_t chipId = ESP.getChipId();
  #else
  uint32_t chipId = random(0xFFFF);
  #endif
  
  char apSSID[VWIRE_PROV_MAX_SSID_LEN];
  snprintf(apSSID, sizeof(apSSID), "%s%04X", VWIRE_PROV_AP_PREFIX, (uint16_t)(chipId & 0xFFFF));
  
  return startAPMode(apSSID, apPassword, timeout);
}

bool VwireProvisioningClass::startAPMode(const char* apSSID, const char* apPassword, unsigned long timeout) {
  // Stop any existing provisioning
  stop();
  
  _debugPrintf("[Provision] Starting AP Mode: %s", apSSID);
  
  // Store AP SSID for retrieval
  strncpy(_apSSID, apSSID, VWIRE_PROV_MAX_SSID_LEN - 1);
  
  // Disconnect and set to AP mode
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Start AP
  bool started;
  if (apPassword && strlen(apPassword) >= 8) {
    started = WiFi.softAP(apSSID, apPassword);
    _debugPrintf("[Provision] AP started with password");
  } else {
    started = WiFi.softAP(apSSID);
    _debugPrintf("[Provision] AP started (open network)");
  }
  
  if (!started) {
    _debugPrint("[Provision] Failed to start AP!");
    return false;
  }
  
  delay(100);
  
  _debugPrintf("[Provision] AP IP: %s", WiFi.softAPIP().toString().c_str());
  
  // Setup web server
  _setupAPWebServer();
  
  _state = VWIRE_PROV_AP_ACTIVE;
  _method = VWIRE_PROV_METHOD_AP;
  _startTime = millis();
  _timeout = timeout;
  
  _setState(VWIRE_PROV_AP_ACTIVE);
  
  return true;
}

void VwireProvisioningClass::stopAPMode() {
  if (_method == VWIRE_PROV_METHOD_AP) {
    if (_webServer) {
      _webServer->stop();
      delete _webServer;
      _webServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    _state = VWIRE_PROV_IDLE;
    _method = VWIRE_PROV_METHOD_NONE;
    _debugPrint("[Provision] AP Mode stopped");
  }
}

const char* VwireProvisioningClass::getAPSSID() {
  return _apSSID;
}

String VwireProvisioningClass::getAPIP() {
  return WiFi.softAPIP().toString();
}

void VwireProvisioningClass::_setupAPWebServer() {
  if (_webServer) {
    delete _webServer;
  }
  
  #if defined(VWIRE_BOARD_ESP32)
  _webServer = new WebServer(VWIRE_PROV_WEB_PORT);
  #elif defined(VWIRE_BOARD_ESP8266)
  _webServer = new ESP8266WebServer(VWIRE_PROV_WEB_PORT);
  #endif
  
  // Use lambdas to wrap member functions
  _webServer->on("/", HTTP_GET, [this]() { this->_handleRoot(); });
  _webServer->on("/config", HTTP_POST, [this]() { this->_handleConfig(); });
  _webServer->on("/status", HTTP_GET, [this]() { this->_handleStatus(); });
    // Handshake endpoint: app checks if device is ready
    _webServer->on("/handshake", HTTP_GET, [this]() {
      _webServer->send(200, "application/json", "{\"status\":\"ready\"}");
    });

    // Confirmation endpoint: app polls until credentials are received
    _webServer->on("/confirm", HTTP_GET, [this]() {
      bool received = _credentialsLoaded; // or use a more robust flag if needed
      _webServer->send(200, "application/json", String("{\"received\":") + (received ? "true" : "false") + "}");
    });
  _webServer->onNotFound([this]() { this->_handleNotFound(); });
  
  _webServer->begin();
  _debugPrint("[Provision] Web server started on port 80");
}

void VwireProvisioningClass::_handleRoot() {
  String html = FPSTR(HTML_HEADER);
  html += FPSTR(HTML_CONFIG_FORM);
  html += FPSTR(HTML_FOOTER);
  _webServer->send(200, "text/html", html);
}

void VwireProvisioningClass::_handleConfig() {
  _debugPrint("[Provision] Received configuration request");
  
  // Extract credentials. Prefer form-encoded fields, but fall back to JSON body.
  String ssid;
  String password;
  String token;

  if (_webServer->hasArg("ssid") || _webServer->hasArg("token")) {
    // Form-encoded or query params
    ssid = _webServer->arg("ssid");
    password = _webServer->arg("password");
    token = _webServer->arg("token");
  } else {
    // Try parse JSON body (support both {ssid,password,token} and {wifi_ssid,wifi_pass,token})
    String body = _webServer->arg("plain");
    if (body.length() > 0) {
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, body);
      if (!err) {
        if (doc.containsKey("ssid")) ssid = String((const char*)doc["ssid"]);
        else if (doc.containsKey("wifi_ssid")) ssid = String((const char*)doc["wifi_ssid"]);

        if (doc.containsKey("password")) password = String((const char*)doc["password"]);
        else if (doc.containsKey("wifi_pass")) password = String((const char*)doc["wifi_pass"]);

        if (doc.containsKey("token")) token = String((const char*)doc["token"]);
      } else {
        _webServer->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON body\"}");
        return;
      }
    }
  }
  
  if (ssid.length() == 0) {
    _webServer->send(400, "application/json", "{\"success\":false,\"error\":\"SSID is required\"}");
    return;
  }

  if (token.length() == 0) {
    _webServer->send(400, "application/json", "{\"success\":false,\"error\":\"Device token is required\"}");
    return;
  }
  
  _debugPrintf("[Provision] Received - SSID: %s, Token: %s", ssid.c_str(), token.c_str());
  
  // Save credentials
  if (!saveCredentials(ssid.c_str(), password.c_str(), token.c_str())) {
    _webServer->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save credentials\"}");
    return;
  }

  // Mark handshake confirmation
  _handshakeConfirmed = true;

  // Send success response
  _webServer->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");

  // Call credentials callback
  if (_credentialsCallback) {
    _credentialsCallback(_credentials.ssid, _credentials.password, _credentials.authToken);
  }

  // Do NOT stop AP or connect yet; wait for explicit confirmation from app
}

void VwireProvisioningClass::_handleStatus() {
  String json = "{";
  json += "\"state\":\"" + String((int)_state) + "\",";
  json += "\"method\":\"ap\",";
  json += "\"apSSID\":\"" + String(_apSSID) + "\",";
  json += "\"apIP\":\"" + getAPIP() + "\"";
  json += "}";
  _webServer->send(200, "application/json", json);
}

void VwireProvisioningClass::_handleNotFound() {
  _webServer->send(404, "text/plain", "Not found");
}

#endif // VWIRE_HAS_AP_PROVISIONING

// =============================================================================
// COMMON OPERATIONS
// =============================================================================

void VwireProvisioningClass::run() {
  #if VWIRE_HAS_AP_PROVISIONING
  if (_method == VWIRE_PROV_METHOD_AP && _webServer) {
    _webServer->handleClient();
    
    // Only proceed to stop AP and connect after handshake confirmation
    if (_handshakeConfirmed) {
      stopAPMode();
      WiFi.mode(WIFI_STA);
      delay(500);
      if (_connectToWiFi()) {
        _setState(VWIRE_PROV_SUCCESS);
        _debugPrint("[Provision] AP Mode provisioning successful!");
      } else {
        _setState(VWIRE_PROV_FAILED);
        _debugPrint("[Provision] AP Mode failed - could not connect to WiFi");
      }
      _handshakeConfirmed = false; // reset for next provisioning
    }

    // Check for timeout
    if (_timeout > 0 && (millis() - _startTime) >= _timeout) {
      _debugPrint("[Provision] AP Mode timeout!");
      stopAPMode();
      _setState(VWIRE_PROV_TIMEOUT);
    }
  }
  #endif
}

void VwireProvisioningClass::stop() {
  #if VWIRE_HAS_AP_PROVISIONING
  if (_method == VWIRE_PROV_METHOD_AP) {
    stopAPMode();
  }
  #endif
  
  _state = VWIRE_PROV_IDLE;
  _method = VWIRE_PROV_METHOD_NONE;
}

VwireProvisioningState VwireProvisioningClass::getState() {
  return _state;
}

VwireProvisioningMethod VwireProvisioningClass::getMethod() {
  return _method;
}

bool VwireProvisioningClass::isProvisioning() {
  return _state == VWIRE_PROV_AP_ACTIVE ||
         _state == VWIRE_PROV_CONNECTING;
}

void VwireProvisioningClass::_setState(VwireProvisioningState state) {
  if (_state != state) {
    _state = state;
    if (_stateCallback) {
      _stateCallback(state);
    }
  }
}

bool VwireProvisioningClass::_connectToWiFi(unsigned long timeout) {
  _debugPrintf("[Provision] Connecting to WiFi: %s", _credentials.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(_credentials.ssid, _credentials.password);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _debugPrint(".");
    yield();
    
    if (millis() - start >= timeout) {
      _debugPrint("\n[Provision] WiFi connection timeout!");
      return false;
    }
  }
  
  _debugPrintf("\n[Provision] WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

// =============================================================================
// CALLBACKS
// =============================================================================

void VwireProvisioningClass::onStateChange(ProvisioningStateCallback callback) {
  _stateCallback = callback;
}

void VwireProvisioningClass::onCredentialsReceived(CredentialsReceivedCallback callback) {
  _credentialsCallback = callback;
}

void VwireProvisioningClass::onProgress(ProvisioningProgressCallback callback) {
  _progressCallback = callback;
}

// =============================================================================
// DEBUG
// =============================================================================

void VwireProvisioningClass::setDebug(bool enable) {
  _debug = enable;
}

void VwireProvisioningClass::setDebugStream(Stream& stream) {
  _debugStream = &stream;
}

void VwireProvisioningClass::_debugPrint(const char* message) {
  if (_debug && _debugStream) {
    _debugStream->println(message);
  }
}

void VwireProvisioningClass::_debugPrintf(const char* format, ...) {
  if (_debug && _debugStream) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    _debugStream->println(buffer);
  }
}
