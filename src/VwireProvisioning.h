/*
 * Vwire IOT Arduino Library - WiFi Provisioning
 * 
 * Provides WiFi provisioning support for ESP32 and ESP8266 devices:
 * - SmartConfig (ESP-Touch): Receive WiFi credentials + device token from mobile app
 * - AP Mode: Device creates hotspot, user connects and sends configuration
 * 
 * Both methods receive:
 * - WiFi SSID and password
 * - Device authentication token (from Vwire dashboard)
 * 
 * Usage:
 *   #include <VwireProvisioning.h>
 *   
 *   // In setup():
 *   if (!VwireProvision.hasCredentials()) {
 *     VwireProvision.startSmartConfig();  // or startAPMode()
 *   }
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_PROVISIONING_H
#define VWIRE_PROVISIONING_H

#include <Arduino.h>
#include "VwireConfig.h"

// SmartConfig is only available on ESP32 and ESP8266
#if defined(VWIRE_BOARD_ESP32) || defined(VWIRE_BOARD_ESP8266)
  #define VWIRE_HAS_SMARTCONFIG 1
#else
  #define VWIRE_HAS_SMARTCONFIG 0
#endif

// AP Mode provisioning available on all WiFi boards
#if VWIRE_HAS_WIFI
  #define VWIRE_HAS_AP_PROVISIONING 1
#else
  #define VWIRE_HAS_AP_PROVISIONING 0
#endif

#if defined(VWIRE_BOARD_ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <Preferences.h>
  #include <esp_smartconfig.h>
#elif defined(VWIRE_BOARD_ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <EEPROM.h>
#endif

// =============================================================================
// CONFIGURATION
// =============================================================================

/** @brief Maximum SSID length (including null terminator) */
#define VWIRE_PROV_MAX_SSID_LEN 33

/** @brief Maximum password length (including null terminator) */
#define VWIRE_PROV_MAX_PASS_LEN 65

/** @brief Maximum auth token length (including null terminator) */
#define VWIRE_PROV_MAX_TOKEN_LEN 64

/** @brief Default AP SSID prefix */
#define VWIRE_PROV_AP_PREFIX "VWire_Setup_"

/** @brief Default SmartConfig timeout (ms) */
#define VWIRE_PROV_SMARTCONFIG_TIMEOUT 120000  // 2 minutes

/** @brief Default AP Mode timeout (ms, 0 = no timeout) */
#define VWIRE_PROV_AP_TIMEOUT 0

/** @brief AP Mode Web server port */
#define VWIRE_PROV_WEB_PORT 80

/** @brief EEPROM/Preferences namespace for credentials */
#define VWIRE_PROV_NAMESPACE "vwire_cred"

/** @brief EEPROM start address for ESP8266 */
#define VWIRE_PROV_EEPROM_START 0

/** @brief EEPROM size for ESP8266 */
#define VWIRE_PROV_EEPROM_SIZE 256

/** @brief Magic value to validate stored credentials */
#define VWIRE_PROV_MAGIC 0xVW10

// =============================================================================
// PROVISIONING STATE
// =============================================================================

/**
 * @brief Provisioning state machine states
 */
typedef enum {
  VWIRE_PROV_IDLE = 0,           ///< Not provisioning
  VWIRE_PROV_SMARTCONFIG_WAIT,   ///< Waiting for SmartConfig data
  VWIRE_PROV_AP_ACTIVE,          ///< AP Mode active, waiting for config
  VWIRE_PROV_CONNECTING,         ///< Received credentials, connecting to WiFi
  VWIRE_PROV_SUCCESS,            ///< Provisioning successful
  VWIRE_PROV_FAILED,             ///< Provisioning failed
  VWIRE_PROV_TIMEOUT             ///< Provisioning timed out
} VwireProvisioningState;

/**
 * @brief Provisioning method
 */
typedef enum {
  VWIRE_PROV_METHOD_NONE = 0,
  VWIRE_PROV_METHOD_SMARTCONFIG,
  VWIRE_PROV_METHOD_AP
} VwireProvisioningMethod;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/** @brief Callback when provisioning state changes */
typedef void (*ProvisioningStateCallback)(VwireProvisioningState state);

/** @brief Callback when credentials are received */
typedef void (*CredentialsReceivedCallback)(const char* ssid, const char* password, const char* token);

/** @brief Callback for provisioning progress (SmartConfig) */
typedef void (*ProvisioningProgressCallback)(int progress);

// =============================================================================
// STORED CREDENTIALS STRUCTURE
// =============================================================================

/**
 * @brief Structure for stored WiFi credentials and device token
 */
struct VwireCredentials {
  uint16_t magic;                          ///< Magic value to validate data
  char ssid[VWIRE_PROV_MAX_SSID_LEN];      ///< WiFi SSID
  char password[VWIRE_PROV_MAX_PASS_LEN];  ///< WiFi password
  char authToken[VWIRE_PROV_MAX_TOKEN_LEN];///< Device auth token from Vwire
  uint8_t checksum;                         ///< Simple checksum
  
  /** @brief Calculate checksum */
  uint8_t calcChecksum() const {
    uint8_t sum = 0;
    const uint8_t* data = (const uint8_t*)this;
    // Checksum everything except the checksum byte itself
    for (size_t i = 0; i < sizeof(VwireCredentials) - 1; i++) {
      sum ^= data[i];
    }
    return sum;
  }
  
  /** @brief Verify checksum */
  bool isValid() const {
    return magic == VWIRE_PROV_MAGIC && checksum == calcChecksum();
  }
};

// =============================================================================
// MAIN PROVISIONING CLASS
// =============================================================================

/**
 * @brief WiFi provisioning manager for Vwire IOT devices
 * 
 * Supports two provisioning methods:
 * 
 * 1. SmartConfig (ESP-Touch):
 *    - Device listens for encoded WiFi packets from mobile app
 *    - Receives SSID, password, and device token
 *    - No user interaction needed on device
 * 
 * 2. AP Mode:
 *    - Device creates WiFi hotspot "VWire_Setup_XXXX"
 *    - User connects phone to hotspot
 *    - Mobile app sends configuration via HTTP
 * 
 * Credentials are stored persistently and can be retrieved for Vwire connection.
 * 
 * @code
 * // Example usage
 * void setup() {
 *   Serial.begin(115200);
 *   
 *   // Check if we have stored credentials
 *   if (VwireProvision.hasCredentials()) {
 *     // Use stored credentials
 *     Vwire.config(VwireProvision.getAuthToken());
 *     Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword());
 *   } else {
 *     // Start provisioning
 *     VwireProvision.startSmartConfig();  // or startAPMode()
 *     
 *     while (VwireProvision.getState() != VWIRE_PROV_SUCCESS) {
 *       VwireProvision.run();
 *       delay(100);
 *     }
 *     
 *     // Provisioning complete, restart or continue
 *     ESP.restart();
 *   }
 * }
 * @endcode
 */
class VwireProvisioningClass {
public:
  /** @brief Constructor */
  VwireProvisioningClass();
  
  /** @brief Destructor */
  ~VwireProvisioningClass();
  
  // =========================================================================
  // STORED CREDENTIALS MANAGEMENT
  // =========================================================================
  
  /**
   * @brief Check if valid credentials are stored
   * @return true if credentials exist and are valid
   */
  bool hasCredentials();
  
  /**
   * @brief Load credentials from persistent storage
   * @return true if loaded successfully
   */
  bool loadCredentials();
  
  /**
   * @brief Save credentials to persistent storage
   * @param ssid WiFi SSID
   * @param password WiFi password
   * @param authToken Device auth token
   * @return true if saved successfully
   */
  bool saveCredentials(const char* ssid, const char* password, const char* authToken);
  
  /**
   * @brief Clear stored credentials
   * @return true if cleared successfully
   */
  bool clearCredentials();
  
  /**
   * @brief Get stored WiFi SSID
   * @return SSID string (empty if not stored)
   */
  const char* getSSID();
  
  /**
   * @brief Get stored WiFi password
   * @return Password string (empty if not stored)
   */
  const char* getPassword();
  
  /**
   * @brief Get stored auth token
   * @return Auth token string (empty if not stored)
   */
  const char* getAuthToken();
  
  // =========================================================================
  // SMARTCONFIG PROVISIONING
  // =========================================================================
  
  #if VWIRE_HAS_SMARTCONFIG
  /**
   * @brief Start SmartConfig provisioning
   * 
   * Puts device in SmartConfig mode to receive WiFi credentials
   * and device token from the Vwire mobile app.
   * 
   * @param timeout Timeout in milliseconds (default: 120000 = 2 min)
   * @return true if started successfully
   */
  bool startSmartConfig(unsigned long timeout = VWIRE_PROV_SMARTCONFIG_TIMEOUT);
  
  /**
   * @brief Stop SmartConfig provisioning
   */
  void stopSmartConfig();
  #endif
  
  // =========================================================================
  // AP MODE PROVISIONING
  // =========================================================================
  
  #if VWIRE_HAS_AP_PROVISIONING
  /**
   * @brief Start AP Mode provisioning
   * 
   * Creates WiFi hotspot "VWire_Setup_XXXX" (XXXX = chip ID).
   * User connects to this network and opens mobile app to configure.
   * 
   * @param apPassword Optional AP password (empty = open network)
   * @param timeout Timeout in milliseconds (0 = no timeout)
   * @return true if started successfully
   */
  bool startAPMode(const char* apPassword = "", unsigned long timeout = VWIRE_PROV_AP_TIMEOUT);
  
  /**
   * @brief Start AP Mode with custom AP name
   * @param apSSID Custom AP SSID
   * @param apPassword Optional AP password
   * @param timeout Timeout in milliseconds
   * @return true if started successfully
   */
  bool startAPMode(const char* apSSID, const char* apPassword, unsigned long timeout);
  
  /**
   * @brief Stop AP Mode provisioning
   */
  void stopAPMode();
  
  /**
   * @brief Get AP Mode SSID (for display)
   * @return AP SSID string
   */
  const char* getAPSSID();
  
  /**
   * @brief Get device's IP address in AP mode
   * @return IP address string
   */
  String getAPIP();
  #endif
  
  // =========================================================================
  // COMMON OPERATIONS
  // =========================================================================
  
  /**
   * @brief Process provisioning (call in loop)
   * @note Must be called frequently during provisioning
   */
  void run();
  
  /**
   * @brief Stop any active provisioning
   */
  void stop();
  
  /**
   * @brief Get current provisioning state
   * @return VwireProvisioningState
   */
  VwireProvisioningState getState();
  
  /**
   * @brief Get current provisioning method
   * @return VwireProvisioningMethod
   */
  VwireProvisioningMethod getMethod();
  
  /**
   * @brief Check if provisioning is in progress
   * @return true if actively provisioning
   */
  bool isProvisioning();
  
  // =========================================================================
  // CALLBACKS
  // =========================================================================
  
  /**
   * @brief Set callback for state changes
   * @param callback Function to call on state change
   */
  void onStateChange(ProvisioningStateCallback callback);
  
  /**
   * @brief Set callback for when credentials are received
   * @param callback Function to call with credentials
   */
  void onCredentialsReceived(CredentialsReceivedCallback callback);
  
  /**
   * @brief Set callback for provisioning progress
   * @param callback Function to call with progress (0-100)
   */
  void onProgress(ProvisioningProgressCallback callback);
  
  // =========================================================================
  // DEBUG
  // =========================================================================
  
  /**
   * @brief Enable debug output
   * @param enable true to enable
   */
  void setDebug(bool enable);
  
  /**
   * @brief Set debug output stream
   * @param stream Stream for debug output (default: Serial)
   */
  void setDebugStream(Stream& stream);

private:
  // State
  VwireProvisioningState _state;
  VwireProvisioningMethod _method;
  VwireCredentials _credentials;
  bool _credentialsLoaded;
  
  // Timeouts
  unsigned long _startTime;
  unsigned long _timeout;
  
  // Callbacks
  ProvisioningStateCallback _stateCallback;
  CredentialsReceivedCallback _credentialsCallback;
  ProvisioningProgressCallback _progressCallback;
  
  // Debug
  bool _debug;
  Stream* _debugStream;
  
  // AP Mode
  #if VWIRE_HAS_AP_PROVISIONING
  char _apSSID[VWIRE_PROV_MAX_SSID_LEN];
  #if defined(VWIRE_BOARD_ESP32)
  WebServer* _webServer;
  #elif defined(VWIRE_BOARD_ESP8266)
  ESP8266WebServer* _webServer;
  #endif
  void _setupAPWebServer();
  void _handleRoot();
  void _handleConfig();
  void _handleStatus();
  void _handleNotFound();
  static VwireProvisioningClass* _instance;
  #endif
  
  // SmartConfig
  #if VWIRE_HAS_SMARTCONFIG
  void _processSmartConfig();
  #endif
  
  // Internal helpers
  void _setState(VwireProvisioningState state);
  bool _connectToWiFi(unsigned long timeout = 30000);
  void _debugPrint(const char* message);
  void _debugPrintf(const char* format, ...);
  
  // Storage helpers
  #if defined(VWIRE_BOARD_ESP32)
  Preferences _preferences;
  #endif
  bool _loadFromStorage();
  bool _saveToStorage();
  bool _clearStorage();
};

// =============================================================================
// GLOBAL INSTANCE
// =============================================================================
extern VwireProvisioningClass VwireProvision;

#endif // VWIRE_PROVISIONING_H
