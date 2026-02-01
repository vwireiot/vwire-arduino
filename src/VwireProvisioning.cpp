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

// Form generation is now done dynamically in _setupAPWebServer() based on _oemMode

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
  , _oemMode(false)
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
  return _credentials.isValid();
}

bool VwireProvisioningClass::loadCredentials() {
  _credentialsLoaded = true;
  return _loadFromStorage();
}

bool VwireProvisioningClass::saveCredentials(const char* ssid, const char* password, const char* authToken) {
  // Clear and initialize fresh structure
  memset(&_credentials, 0, sizeof(_credentials));
  _credentials.magic = VWIRE_PROV_MAGIC;
  
  // Copy SSID (required)
  if (ssid && strlen(ssid) > 0) {
    size_t len = strlen(ssid);
    if (len >= VWIRE_PROV_MAX_SSID_LEN) len = VWIRE_PROV_MAX_SSID_LEN - 1;
    memcpy(_credentials.ssid, ssid, len);
    _credentials.ssid[len] = '\0';
  } else {
    _debugPrint("[Provision] ERROR: SSID is required!");
    return false;
  }
  
  // Copy password (optional)
  if (password && strlen(password) > 0) {
    size_t len = strlen(password);
    if (len >= VWIRE_PROV_MAX_PASS_LEN) len = VWIRE_PROV_MAX_PASS_LEN - 1;
    memcpy(_credentials.password, password, len);
    _credentials.password[len] = '\0';
  }
  
  // Copy token (optional - empty in OEM mode)
  if (authToken && strlen(authToken) > 0) {
    size_t len = strlen(authToken);
    if (len >= VWIRE_PROV_MAX_TOKEN_LEN) len = VWIRE_PROV_MAX_TOKEN_LEN - 1;
    memcpy(_credentials.authToken, authToken, len);
    _credentials.authToken[len] = '\0';
  }
  
  // Calculate checksum AFTER all data is set
  _credentials.checksum = _credentials.calcChecksum();
  _credentialsLoaded = true;
  
  _debugPrintf("[Provision] Saving - SSID:%s (%d) Pass:%d Token:%d chars", 
               _credentials.ssid, strlen(_credentials.ssid),
               strlen(_credentials.password), strlen(_credentials.authToken));
  
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
// ESP32 uses Preferences library (NVS) - robust implementation
bool VwireProvisioningClass::_loadFromStorage() {
  if (!_preferences.begin(VWIRE_PROV_NAMESPACE, true)) {  // Read-only
    _debugPrint("[Provision] Failed to open NVS namespace");
    return false;
  }
  
  size_t len = _preferences.getBytes("cred", &_credentials, sizeof(_credentials));
  _preferences.end();
  
  if (len != sizeof(_credentials)) {
    _debugPrintf("[Provision] NVS size mismatch: %d vs %d", len, sizeof(_credentials));
    _credentials.init();
    return false;
  }
  
  if (_credentials.magic != VWIRE_PROV_MAGIC) {
    _debugPrintf("[Provision] NVS magic mismatch: 0x%04X", _credentials.magic);
    _credentials.init();
    return false;
  }
  
  uint8_t storedChecksum = _credentials.checksum;
  uint8_t calcedChecksum = _credentials.calcChecksum();
  if (storedChecksum != calcedChecksum) {
    _debugPrintf("[Provision] NVS checksum mismatch: 0x%02X vs 0x%02X", storedChecksum, calcedChecksum);
    _credentials.init();
    return false;
  }
  
  _debugPrintf("[Provision] Loaded from NVS - SSID:%s Token:%d chars", 
               _credentials.ssid, strlen(_credentials.authToken));
  return true;
}

bool VwireProvisioningClass::_saveToStorage() {
  // Ensure checksum is current
  _credentials.checksum = _credentials.calcChecksum();
  
  if (!_preferences.begin(VWIRE_PROV_NAMESPACE, false)) {  // Read-write
    _debugPrint("[Provision] Failed to open NVS for write");
    return false;
  }
  
  size_t written = _preferences.putBytes("cred", &_credentials, sizeof(_credentials));
  _preferences.end();
  
  if (written != sizeof(_credentials)) {
    _debugPrintf("[Provision] NVS write failed: %d vs %d", written, sizeof(_credentials));
    return false;
  }
  
  _debugPrintf("[Provision] Saved to NVS - SSID:%s Token:%d chars", 
               _credentials.ssid, strlen(_credentials.authToken));
  return true;
}

bool VwireProvisioningClass::_clearStorage() {
  if (!_preferences.begin(VWIRE_PROV_NAMESPACE, false)) {
    return false;
  }
  _preferences.clear();
  _preferences.end();
  _debugPrint("[Provision] NVS credentials cleared");
  return true;
}

#elif defined(VWIRE_BOARD_ESP8266)
// ESP8266 uses EEPROM - robust implementation
bool VwireProvisioningClass::_loadFromStorage() {
  // Clear structure first
  memset(&_credentials, 0, sizeof(_credentials));
  
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  // Read structure from EEPROM byte by byte
  uint8_t* ptr = (uint8_t*)&_credentials;
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    ptr[i] = EEPROM.read(VWIRE_PROV_EEPROM_START + i);
  }
  
  EEPROM.end();
  
  // Validate magic number
  if (_credentials.magic != VWIRE_PROV_MAGIC) {
    _debugPrintf("[Provision] EEPROM magic mismatch: 0x%04X (expected 0x%04X)", _credentials.magic, VWIRE_PROV_MAGIC);
    _credentials.init();
    return false;
  }
  
  // Ensure null termination (safety)
  _credentials.ssid[VWIRE_PROV_MAX_SSID_LEN - 1] = '\0';
  _credentials.password[VWIRE_PROV_MAX_PASS_LEN - 1] = '\0';
  _credentials.authToken[VWIRE_PROV_MAX_TOKEN_LEN - 1] = '\0';
  
  // Validate checksum
  uint8_t storedChecksum = _credentials.checksum;
  uint8_t calcedChecksum = _credentials.calcChecksum();
  if (storedChecksum != calcedChecksum) {
    _debugPrintf("[Provision] EEPROM checksum mismatch: 0x%02X vs 0x%02X", storedChecksum, calcedChecksum);
    _debugPrintf("[Provision] Raw SSID len:%d Pass len:%d Token len:%d",
                 strlen(_credentials.ssid), strlen(_credentials.password), strlen(_credentials.authToken));
    _credentials.init();
    return false;
  }
  
  // Validate SSID is not empty
  if (strlen(_credentials.ssid) == 0) {
    _debugPrint("[Provision] EEPROM SSID is empty");
    _credentials.init();
    return false;
  }
  
  _debugPrintf("[Provision] Loaded from EEPROM OK - SSID:%s (%d) Pass:%d Token:%d", 
               _credentials.ssid, strlen(_credentials.ssid),
               strlen(_credentials.password), strlen(_credentials.authToken));
  return true;
}

bool VwireProvisioningClass::_saveToStorage() {
  // Verify checksum before saving
  _credentials.checksum = _credentials.calcChecksum();
  
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  // Write structure to EEPROM
  uint8_t* ptr = (uint8_t*)&_credentials;
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    EEPROM.write(VWIRE_PROV_EEPROM_START + i, ptr[i]);
  }
  
  bool result = EEPROM.commit();
  
  if (!result) {
    EEPROM.end();
    _debugPrint("[Provision] EEPROM commit failed!");
    return false;
  }
  
  // Verify write by reading back
  VwireCredentials verify;
  uint8_t* vptr = (uint8_t*)&verify;
  for (size_t i = 0; i < sizeof(verify); i++) {
    vptr[i] = EEPROM.read(VWIRE_PROV_EEPROM_START + i);
  }
  
  EEPROM.end();
  
  // Compare what we wrote vs what we read
  if (memcmp(&_credentials, &verify, sizeof(_credentials)) != 0) {
    _debugPrint("[Provision] EEPROM verify failed - data mismatch!");
    return false;
  }
  
  _debugPrintf("[Provision] Saved to EEPROM - SSID:%s Token:%d chars (verified)", 
               _credentials.ssid, strlen(_credentials.authToken));
  return true;
}

bool VwireProvisioningClass::_clearStorage() {
  EEPROM.begin(VWIRE_PROV_EEPROM_SIZE);
  
  // Write zeros to entire credential area
  for (size_t i = 0; i < sizeof(_credentials); i++) {
    EEPROM.write(VWIRE_PROV_EEPROM_START + i, 0);
  }
  
  bool result = EEPROM.commit();
  EEPROM.end();
  
  _debugPrint("[Provision] EEPROM credentials cleared");
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

bool VwireProvisioningClass::startAPMode(const char* apPassword, unsigned long timeout, bool oemMode) {
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
  
  return startAPMode(apSSID, apPassword, timeout, oemMode);
}

bool VwireProvisioningClass::startAPMode(const char* apSSID, const char* apPassword, unsigned long timeout, bool oemMode) {
  // Stop any existing provisioning
  stop();
  
  // Store OEM mode flag
  _oemMode = oemMode;
  
  _debugPrintf("[Provision] Starting AP Mode: %s (OEM Mode: %s)", apSSID, oemMode ? "YES" : "NO");
  
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
  
  // Generate form based on OEM mode
  html += F("<div class=\"logo\">\n");
  html += F("  <h1>VWire Setup</h1>\n");
  html += F("  <p>Configure your IoT device</p>\n");
  html += F("</div>\n\n");
  html += F("<form id=\"configForm\" onsubmit=\"return submitConfig()\">\n");
  
  // WiFi SSID field (always shown)
  html += F("  <div class=\"form-group\">\n");
  html += F("    <label for=\"ssid\">WiFi Network (SSID)</label>\n");
  html += F("    <input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"Your WiFi name\" required maxlength=\"32\">\n");
  html += F("  </div>\n\n");
  
  // WiFi Password field (always shown)
  html += F("  <div class=\"form-group\">\n");
  html += F("    <label for=\"password\">WiFi Password</label>\n");
  html += F("    <input type=\"password\" id=\"password\" name=\"password\" placeholder=\"WiFi password\" maxlength=\"64\">\n");
  html += F("  </div>\n\n");
  
  // Device Token field (only for end-user mode)
  if (!_oemMode) {
    html += F("  <div class=\"form-group\">\n");
    html += F("    <label for=\"token\">Device Token</label>\n");
    html += F("    <input type=\"text\" id=\"token\" name=\"token\" placeholder=\"From VWire dashboard\" required maxlength=\"63\">\n");
    html += F("  </div>\n\n");
  }
  
  html += F("  <button type=\"submit\" id=\"submitBtn\">Configure Device</button>\n\n");
  html += F("  <div id=\"status\" class=\"status\" style=\"display:none;\"></div>\n\n");
  html += F("  <div class=\"note\">\n");
  html += F("    <strong>Note:</strong> After configuration, the device will restart and connect to your WiFi network.\n");
  html += F("    You can then close this page.\n");
  html += F("  </div>\n");
  html += F("</form>\n\n");
  
  // JavaScript for form submission
  html += F("<script>\n");
  html += F("function submitConfig() {\n");
  html += F("  var btn = document.getElementById('submitBtn');\n");
  html += F("  var status = document.getElementById('status');\n");
  html += F("  var ssid = document.getElementById('ssid').value;\n");
  html += F("  var password = document.getElementById('password').value;\n");
  
  if (!_oemMode) {
    html += F("  var token = document.getElementById('token').value;\n");
  }
  
  html += F("  btn.disabled = true;\n");
  html += F("  btn.innerHTML = '<span class=\"spinner\"></span>Configuring...';\n");
  html += F("  status.style.display = 'none';\n\n");
  
  html += F("  var body = 'ssid=' + encodeURIComponent(ssid) + \n");
  html += F("             '&password=' + encodeURIComponent(password)");
  
  if (!_oemMode) {
    html += F(" + \n             '&token=' + encodeURIComponent(token)");
  }
  
  html += F(";\n\n");
  
  html += F("  fetch('/config', {\n");
  html += F("    method: 'POST',\n");
  html += F("    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n");
  html += F("    body: body\n");
  html += F("  })\n");
  html += F("  .then(function(response) { return response.json(); })\n");
  html += F("  .then(function(data) {\n");
  html += F("    if (data.success) {\n");
  html += F("      status.className = 'status success';\n");
  html += F("      status.innerHTML = '✓ Configuration saved! Device is restarting...';\n");
  html += F("      status.style.display = 'block';\n");
  html += F("    } else {\n");
  html += F("      throw new Error(data.error || 'Configuration failed');\n");
  html += F("    }\n");
  html += F("  })\n");
  html += F("  .catch(function(error) {\n");
  html += F("    status.className = 'status error';\n");
  html += F("    status.innerHTML = '✗ ' + error.message;\n");
  html += F("    status.style.display = 'block';\n");
  html += F("    btn.disabled = false;\n");
  html += F("    btn.innerHTML = 'Configure Device';\n");
  html += F("  });\n\n");
  
  html += F("  return false;\n");
  html += F("}\n");
  html += F("</script>\n");
  
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

  // In OEM mode, token is in firmware - only save WiFi credentials
  // In end-user mode, token comes from form and must be saved
  if (_oemMode) {
    // OEM mode: token is hardcoded in firmware, just save WiFi
    _debugPrintf("[Provision] OEM Mode - Saving WiFi only (token is in firmware)");
    token = "";  // Don't store token in OEM mode
  } else {
    // End-user mode: require token from form
    if (token.length() == 0) {
      _webServer->send(400, "application/json", "{\"success\":false,\"error\":\"Device token is required\"}");
      return;
    }
  }
  
  _debugPrintf("[Provision] Received - SSID: %s, Token: %s", ssid.c_str(), _oemMode ? "(OEM-firmware)" : token.c_str());
  
  // Save credentials (token empty in OEM mode - it's in firmware)
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
