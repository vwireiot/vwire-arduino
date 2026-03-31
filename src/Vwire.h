/*
 * Vwire IOT Arduino Library
 * 
 * Professional IoT platform library for Arduino, ESP32, ESP8266 and compatible boards.
 * Connect your microcontrollers to Vwire IOT cloud platform via MQTT.
 * 
 * Features:
 * - MQTT over TCP (port 1883)
 * - MQTT over TLS (port 8883) - RECOMMENDED for security
 * - Virtual pins for bidirectional communication
 * - Push notifications and email alerts
 * - OTA firmware updates (ESP32/ESP8266)
 * - Auto-reconnection with configurable intervals
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_H
#define VWIRE_H

#include <Arduino.h>
#include "VwireConfig.h"
#include "VwireTimer.h"

// =============================================================================
// PLATFORM-SPECIFIC INCLUDES
// =============================================================================
#if defined(VWIRE_BOARD_ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  
#elif defined(VWIRE_BOARD_ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
  
#elif defined(VWIRE_BOARD_RP2040)
  #include <WiFi.h>
  #include <WiFiClient.h>
  
#elif defined(VWIRE_BOARD_SAMD)
  #include <WiFiNINA.h>
  #include <WiFiClient.h>
  
#else
  #include <WiFi.h>
  #include <WiFiClient.h>
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>

class VwireGPIO;
class VwireReliableDeliveryAddon;
class VwireOTAAddon;

class VwireReliableDelivery : public VwireAddon {
public:
  virtual ~VwireReliableDelivery() {}
  virtual void setEnabled(bool enable) = 0;
  virtual bool isEnabled() const = 0;
  virtual void setAckTimeout(unsigned long timeout) = 0;
  virtual void setMaxRetries(uint8_t retries) = 0;
  virtual void onDeliveryStatus(DeliveryCallback cb) = 0;
  virtual uint8_t getPendingCount() const = 0;
  virtual bool isPending() const = 0;
  virtual bool send(uint8_t pin, const String& value) = 0;
};

class VwireOTAFeature : public VwireAddon {
public:
  virtual ~VwireOTAFeature() {}
  #if VWIRE_HAS_OTA
  virtual void enableLocal(const char* hostname, const char* password) = 0;
  virtual void handle() = 0;
  #endif
  #if VWIRE_ENABLE_CLOUD_OTA
  virtual void enableCloud() = 0;
  virtual void disableCloud() = 0;
  virtual bool isCloudEnabled() const = 0;
  #endif
};

// =============================================================================
// VIRTUAL PIN CLASS
// =============================================================================

/**
 * @brief Represents a virtual pin value received from or sent to the server
 * 
 * VirtualPin wraps string values with convenient type conversion methods.
 * Supports integers, floats, booleans, strings, and comma-separated arrays.
 */
class VirtualPin {
public:
  // =========================================================================
  // CONSTRUCTORS
  // =========================================================================
  
  /** @brief Default constructor - creates empty value */
  VirtualPin() : _value("") {}
  
  /** @brief Construct from String */
  VirtualPin(const String& value) : _value(value) {}
  
  /** @brief Construct from C-string */
  VirtualPin(const char* value) : _value(value) {}
  
  /** @brief Construct from integer */
  VirtualPin(int value) : _value(String(value)) {}
  
  /** @brief Construct from long */
  VirtualPin(long value) : _value(String(value)) {}
  
  /** @brief Construct from unsigned integer */
  VirtualPin(unsigned int value) : _value(String(value)) {}
  
  /** @brief Construct from unsigned long */
  VirtualPin(unsigned long value) : _value(String(value)) {}
  
  /** @brief Construct from float (2 decimal places) */
  VirtualPin(float value) : _value(String(value, 2)) {}
  
  /** @brief Construct from double (4 decimal places) */
  VirtualPin(double value) : _value(String(value, 4)) {}
  
  /** @brief Construct from boolean */
  VirtualPin(bool value) : _value(value ? "1" : "0") {}
  
  // =========================================================================
  // SETTERS
  // =========================================================================
  
  /** @brief Set value from String */
  void set(const String& value) { _value = value; }
  
  /** @brief Set value from C-string */
  void set(const char* value) { _value = String(value); }
  
  /** @brief Set value from integer */
  void set(int value) { _value = String(value); }
  
  /** @brief Set value from long */
  void set(long value) { _value = String(value); }
  
  /** @brief Set value from unsigned integer */
  void set(unsigned int value) { _value = String(value); }
  
  /** @brief Set value from unsigned long */
  void set(unsigned long value) { _value = String(value); }
  
  /** @brief Set value from float (2 decimal places) */
  void set(float value) { _value = String(value, 2); }
  
  /** @brief Set value from double (4 decimal places) */
  void set(double value) { _value = String(value, 4); }
  
  /** @brief Set value from boolean */
  void set(bool value) { _value = value ? "1" : "0"; }
  
  // =========================================================================
  // GETTERS
  // =========================================================================
  
  /** @brief Get value as integer */
  int asInt() const { return _value.toInt(); }
  
  /** @brief Get value as float */
  float asFloat() const { return _value.toFloat(); }
  
  /** @brief Get value as double */
  double asDouble() const { return _value.toDouble(); }
  
  /** @brief Get value as boolean (true for "1", "true", "on") */
  bool asBool() const { return _value == "1" || _value.equalsIgnoreCase("true") || _value.equalsIgnoreCase("on"); }
  
  /** @brief Get value as String */
  String asString() const { return _value; }
  
  /** @brief Get value as C-string */
  const char* asCString() const { return _value.c_str(); }
  
  // =========================================================================
  // ARRAY SUPPORT (comma-separated values)
  // =========================================================================
  
  /**
   * @brief Get number of elements in comma-separated array
   * @return Element count (1 for non-array values)
   */
  int getArraySize() const {
    if (_value.length() == 0) return 0;
    int count = 1;
    for (size_t i = 0; i < _value.length(); i++) {
      if (_value.charAt(i) == ',') count++;
    }
    return count;
  }
  
  /**
   * @brief Get array element as integer
   * @param index Zero-based element index
   * @return Element value as int, 0 if index out of range
   */
  int getArrayInt(int index) const { return getArrayElement(index).toInt(); }
  
  /**
   * @brief Get array element as float
   * @param index Zero-based element index
   * @return Element value as float, 0.0 if index out of range
   */
  float getArrayFloat(int index) const { return getArrayElement(index).toFloat(); }
  
  /**
   * @brief Get array element as string
   * @param index Zero-based element index
   * @return Element value as String, empty if index out of range
   */
  String getArrayElement(int index) const {
    int start = 0;
    int count = 0;
    for (size_t i = 0; i <= _value.length(); i++) {
      if (i == _value.length() || _value.charAt(i) == ',') {
        if (count == index) {
          return _value.substring(start, i);
        }
        count++;
        start = i + 1;
      }
    }
    return "";
  }
  
  // =========================================================================
  // TYPE CONVERSION OPERATORS
  // =========================================================================
  
  /** @brief Implicit conversion to int */
  operator int() const { return asInt(); }
  
  /** @brief Implicit conversion to float */
  operator float() const { return asFloat(); }
  
  /** @brief Implicit conversion to bool */
  operator bool() const { return asBool(); }
  
  /** @brief Implicit conversion to String */
  operator String() const { return _value; }
  
private:
  String _value;
};

// =============================================================================
// SETTINGS STRUCTURE
// =============================================================================

/**
 * @brief Configuration settings for Vwire IOT connection
 * 
 * Contains configurable transport and lifecycle settings for the core client.
 */
struct VwireSettings {
  char authToken[VWIRE_MAX_TOKEN_LENGTH];     ///< Authentication token from dashboard
  char server[VWIRE_MAX_SERVER_LENGTH];       ///< MQTT broker hostname or IP
  uint16_t port;                               ///< MQTT broker port
  VwireTransport transport;                    ///< Transport type (TCP or TLS)
  bool autoReconnect;                          ///< Auto-reconnect on disconnect
  unsigned long reconnectInterval;             ///< Milliseconds between reconnect attempts
  unsigned long heartbeatInterval;             ///< Milliseconds between heartbeats
  unsigned long wifiTimeout;                   ///< WiFi connection timeout (ms)
  unsigned long mqttTimeout;                   ///< MQTT connection timeout (ms)
  uint8_t dataQoS;                             ///< QoS level (note: PubSubClient only supports 0)
  bool dataRetain;                             ///< Retain flag for data writes
  
  /**
   * @brief Default constructor - initializes with safe defaults
   * 
   * Defaults:
   * - Server: mqtt.vwire.io:8883 (TLS)
   * - Auto-reconnect: enabled, 5 second interval
  * - Heartbeat: 15 seconds
   */
  VwireSettings() {
    memset(authToken, 0, sizeof(authToken));
    strncpy(server, VWIRE_DEFAULT_SERVER, VWIRE_MAX_SERVER_LENGTH - 1);
    port = VWIRE_DEFAULT_PORT_TLS;             // Default to secure port
    transport = VWIRE_TRANSPORT_TCP_SSL;       // Default to TLS
    autoReconnect = true;
    dataQoS = 0;                               // PubSubClient only supports QoS 0
    dataRetain = false;                        // Don't retain by default (faster)
    reconnectInterval = VWIRE_DEFAULT_RECONNECT_INTERVAL;
    heartbeatInterval = VWIRE_DEFAULT_HEARTBEAT_INTERVAL;
    wifiTimeout = VWIRE_DEFAULT_WIFI_TIMEOUT;
    mqttTimeout = VWIRE_DEFAULT_MQTT_TIMEOUT;
  }
};

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/** @brief Handler function for virtual pin write events */
typedef void (*PinHandler)(VirtualPin&);

/** @brief Handler function for connection/disconnection events */
typedef void (*ConnectionHandler)();

/** @brief Handler function for raw MQTT messages */
typedef void (*RawMessageHandler)(const char* topic, const char* payload);

// =============================================================================
// AUTO-REGISTRATION SYSTEM
// =============================================================================

/** @brief Maximum number of auto-registered handlers */
#define VWIRE_MAX_AUTO_HANDLERS 32

/**
 * @brief Internal structure for auto-registered pin handlers
 * @note Used internally by VWIRE_RECEIVE macro
 */
struct VwireAutoHandler {
  uint8_t pin;           ///< Virtual pin number
  PinHandler handler;    ///< Handler function pointer
};

// External declarations for the auto-handler system
extern VwireAutoHandler _vwireAutoReceiveHandlers[];
extern uint8_t _vwireAutoReceiveCount;
extern ConnectionHandler _vwireAutoConnectHandler;
extern ConnectionHandler _vwireAutoDisconnectHandler;

// Internal registration functions (called by macros)
void _vwireRegisterReceiveHandler(uint8_t pin, PinHandler handler);
void _vwireRegisterConnectHandler(ConnectionHandler handler);
void _vwireRegisterDisconnectHandler(ConnectionHandler handler);

// =============================================================================
// HANDLER MACROS
// =============================================================================
/**
 * @brief Auto-registration macros for event handlers
 * 
 * These macros provide a convenient way to register handlers that are
 * automatically called when events occur. No manual registration needed.
 * 
 * @code
 * // Handle data received on virtual pin V0
 * VWIRE_RECEIVE(V0) {
 *   int value = param.asInt();  // 'param' is VirtualPin&
 *   digitalWrite(LED_PIN, value);
 * }
 * 
 * // Called when connected to server
 * VWIRE_CONNECTED() {
 *   Serial.println("Connected!");
 *   Vwire.sync(V0, V1);  // Request stored values
 * }
 * 
 * // Called when disconnected
 * VWIRE_DISCONNECTED() {
 *   Serial.println("Disconnected!");
 * }
 * @endcode
 */

// Internal helper macros for unique variable names
#define _VWIRE_CONCAT(a, b) a##b
#define _VWIRE_UNIQUE(prefix, line) _VWIRE_CONCAT(prefix, line)

/**
 * @brief Register handler for virtual pin receive events (data from cloud)
 * @param vpin Virtual pin number (V0-V31 or 0-255)
 * @note The 'param' variable (VirtualPin&) is available inside the handler
 */
#define VWIRE_RECEIVE(vpin) \
  void _vwire_receive_handler_##vpin(VirtualPin& param); \
  struct _VWIRE_UNIQUE(_VwireAutoReg_, __LINE__) { \
    _VWIRE_UNIQUE(_VwireAutoReg_, __LINE__)() { \
      _vwireRegisterReceiveHandler(vpin, _vwire_receive_handler_##vpin); \
    } \
  } _VWIRE_UNIQUE(_vwireAutoRegInstance_, __LINE__); \
  void _vwire_receive_handler_##vpin(VirtualPin& param)

/**
 * @brief Register handler for connection events
 * @note Called after successful MQTT connection
 */
#define VWIRE_CONNECTED() \
  void _vwire_connected_handler(); \
  struct _VWIRE_UNIQUE(_VwireConnectReg_, __LINE__) { \
    _VWIRE_UNIQUE(_VwireConnectReg_, __LINE__)() { \
      _vwireRegisterConnectHandler(_vwire_connected_handler); \
    } \
  } _VWIRE_UNIQUE(_vwireConnectRegInstance_, __LINE__); \
  void _vwire_connected_handler()

/**
 * @brief Register handler for disconnection events
 * @note Called when connection is lost
 */
#define VWIRE_DISCONNECTED() \
  void _vwire_disconnected_handler(); \
  struct _VWIRE_UNIQUE(_VwireDisconnectReg_, __LINE__) { \
    _VWIRE_UNIQUE(_VwireDisconnectReg_, __LINE__)() { \
      _vwireRegisterDisconnectHandler(_vwire_disconnected_handler); \
    } \
  } _VWIRE_UNIQUE(_vwireDisconnectRegInstance_, __LINE__); \
  void _vwire_disconnected_handler()

// =============================================================================
// VIRTUAL PIN DEFINITIONS
// =============================================================================
/**
 * @brief Convenience macros for virtual pins V0-V31
 * @note Pins 0-255 are supported. For pins beyond V31, use the number directly:
 *       Vwire.virtualSend(100, value);
 *       Vwire.virtualSend(255, value);
 */
// =============================================================================
// DIGITAL PIN DEFINITIONS (D0–D13 common, up to D99 via cloud config)
// =============================================================================
// NOTE: These macros produce pin NUMBERS (0–99). The library prefixes "D"
// when building MQTT topics, so D0 maps to topic vwire/{id}/pin/D0.
// They are intentionally separate from virtual pin numbers (V0 etc.).

// Use these with Vwire.gpioWrite("D0", value) or in pin name strings.
// Hardware GPIO numbers are configured via the cloud pinconfig message.

// =============================================================================
// VIRTUAL PIN DEFINITIONS
// =============================================================================

#define V0  0
#define V1  1
#define V2  2
#define V3  3
#define V4  4
#define V5  5
#define V6  6
#define V7  7
#define V8  8
#define V9  9
#define V10 10
#define V11 11
#define V12 12
#define V13 13
#define V14 14
#define V15 15
#define V16 16
#define V17 17
#define V18 18
#define V19 19
#define V20 20
#define V21 21
#define V22 22
#define V23 23
#define V24 24
#define V25 25
#define V26 26
#define V27 27
#define V28 28
#define V29 29
#define V30 30
#define V31 31

// =============================================================================
// MAIN VWIRE CLASS
// =============================================================================

/**
 * @brief Main Vwire IOT client class
 * 
 * Provides connection management, virtual pin operations, notifications,
 * and event handling for the Vwire IOT platform.
 * 
 * @code
 * // Basic usage
 * Vwire.config(AUTH_TOKEN, "mqtt.vwire.io", 8883);
 * Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
 * 
 * void loop() {
 *   Vwire.run();
 *   Vwire.virtualSend(V0, sensorValue);
 * }
 * @endcode
 */
class VwireClass {
public:
  /** @brief Constructor - initializes default settings */
  VwireClass();
  
  /** @brief Destructor - disconnects cleanly */
  ~VwireClass();
  
  // =========================================================================
  // CONFIGURATION METHODS
  // =========================================================================
  
  /**
   * @brief Configure the Vwire connection
   * 
   * Single-call configuration. Uses default Vwire server (mqtt.vwire.io)
   * with TLS encryption.
   * 
   * @param authToken   Authentication token from Vwire IOT dashboard
   * @param deviceId    Device ID from dashboard (e.g., "VW-ABC123"), or nullptr
   *                    to use the auth token as device ID (provisioning flows)
   * @param transport   Transport protocol (default: VWIRE_TRANSPORT_TCP_SSL)
   * 
   * @code
   * // Standard usage — TLS:
   * Vwire.config(AUTH_TOKEN, DEVICE_ID);
   * 
   * // Provisioning (device ID not yet known):
   * Vwire.config(AUTH_TOKEN);
   * 
   * // Plain TCP for boards without SSL support:
   * Vwire.config(AUTH_TOKEN, DEVICE_ID, VWIRE_TRANSPORT_TCP);
   * @endcode
   */
  void config(const char* authToken, const char* deviceId = nullptr,
              VwireTransport transport = VWIRE_TRANSPORT_TCP_SSL);

  /**
   * @brief Set custom device ID (for OEM pre-provisioned devices)
   * 
   * By default, the library uses the auth token as the device ID.
   * For OEM devices with pre-provisioned IDs from the admin panel,
   * use this method to set the custom device ID.
   * 
   * @param deviceId Custom device ID from admin panel (e.g., "VW-ABC123")
   * @note Call this after config() and before begin()
   */
  void setDeviceId(const char* deviceId);
  
  /**
   * @brief Set custom WiFi/network hostname for the device
   * 
   * Sets the hostname used for WiFi (DHCP/mDNS) and local OTA discovery.
   * If not set, defaults to "vwire-<first 8 chars of deviceId>".
   * 
   * When enableOTA() is called:
   * - If a hostname was passed to enableOTA(), it takes priority for OTA
   *   and also sets the WiFi hostname if setHostname() wasn't called.
   * - If no hostname was passed to enableOTA(), the hostname set here is used.
   * 
   * @param hostname Desired hostname (max 32 characters)
   * @note Call before begin() so WiFi uses this hostname from the start
   */
  void setHostname(const char* hostname);
  
  /**
   * @brief Set transport protocol
   * @param transport VWIRE_TRANSPORT_TCP or VWIRE_TRANSPORT_TCP_SSL
   */
  void setTransport(VwireTransport transport);
  
  /**
   * @brief Enable or disable auto-reconnection
   * @param enable true to enable, false to disable
   */
  void setAutoReconnect(bool enable);
  
  /**
   * @brief Set reconnection attempt interval
   * @param interval Milliseconds between reconnect attempts
   */
  void setReconnectInterval(unsigned long interval);
  
  /**
   * @brief Set heartbeat interval
   * @param interval Milliseconds between heartbeats
   */
  void setHeartbeatInterval(unsigned long interval);
  
  /**
   * @brief Set MQTT QoS level (note: PubSubClient only supports QoS 0)
   * @param qos QoS level (0, 1, or 2)
   */
  void setDataQoS(uint8_t qos);
  
  /**
   * @brief Set retain flag for published messages
   * @param retain true to retain messages, false for faster delivery
   */
  void setDataRetain(bool retain);
  
  // =========================================================================
  // RELIABLE DELIVERY CONFIGURATION
  // =========================================================================
  
  /**
   * @brief Enable or disable application-level reliable delivery
   * @param enable true to enable ACK-based delivery
   */
  void setReliableDelivery(bool enable);
  
  /**
   * @brief Set ACK timeout for reliable delivery
   * @param timeout Milliseconds to wait for ACK before retry (default: 5000)
   */
  void setAckTimeout(unsigned long timeout);
  
  /**
   * @brief Set maximum retry attempts for reliable delivery
   * @param retries Number of retries before dropping message (default: 3)
   */
  void setMaxRetries(uint8_t retries);
  
  /**
   * @brief Set callback for delivery status notifications
   * @param cb Callback function (msgId, success)
   */
  void onDeliveryStatus(DeliveryCallback cb);
  
  // =========================================================================
  // CONNECTION METHODS
  // =========================================================================
  
  /**
   * @brief Connect to WiFi and MQTT broker
   * @param ssid WiFi network name
   * @param password WiFi password
   * @return true if connected successfully
   */
  bool begin(const char* ssid, const char* password);
  
  /**
   * @brief Connect using pre-configured WiFi (must be connected already)
   * @return true if MQTT connected successfully
   */
  bool begin();
  
  /**
   * @brief Process MQTT messages and maintain connection
   * @note Must be called frequently in loop()
   */
  void run();
  
  /**
   * @brief Check if connected to MQTT broker
   * @return true if connected
   */
  bool connected();
  
  /**
   * @brief Disconnect from MQTT broker
   */
  void disconnect();
  
  // =========================================================================
  // STATE METHODS
  // =========================================================================
  
  /**
   * @brief Get current connection state
   * @return VwireState enum value
   */
  VwireState getState();
  
  /**
   * @brief Get last error code
   * @return VwireError enum value
   */
  VwireError getLastError();
  
  /**
   * @brief Get WiFi signal strength
   * @return RSSI in dBm (negative value)
   */
  int getWiFiRSSI();
  
  // =========================================================================
  // VIRTUAL PIN SEND OPERATIONS
  // =========================================================================
  
  /**
   * @brief Send value to virtual pin (device → cloud)
   * @tparam T Value type (int, float, bool, String, etc.)
   * @param pin Virtual pin number (0-255)
   * @param value Value to send
   */
  template<typename T>
  void virtualSend(uint8_t pin, T value) {
    VirtualPin vp(value);
    _virtualSendInternal(pin, vp.asString());
  }
  
  /**
   * @brief Send float array to virtual pin (comma-separated)
   * @param pin Virtual pin number
   * @param values Array of float values
   * @param count Number of values
   */
  void virtualSendArray(uint8_t pin, float* values, int count);
  
  /**
   * @brief Send int array to virtual pin (comma-separated)
   * @param pin Virtual pin number
   * @param values Array of int values
   * @param count Number of values
   */
  void virtualSendArray(uint8_t pin, int* values, int count);
  
  /**
   * @brief Send formatted string to virtual pin
   * @param pin Virtual pin number
   * @param format Printf-style format string
   * @param ... Format arguments
   */
  void virtualSendf(uint8_t pin, const char* format, ...);
  
  // =========================================================================
  // SYNC OPERATIONS
  // =========================================================================
  
  /**
   * @brief Request stored value for a pin from server
   * @param pin Virtual pin number
   * @note Server will respond with last known value
   */
  void syncVirtual(uint8_t pin);
  
  /**
   * @brief Request all stored values from server
   */
  void syncAll();
  
  /**
   * @brief Sync multiple specific pins
   * @tparam Pins Pin numbers (variadic)
   * @param pins Pin numbers to sync
   * @code
   * Vwire.sync(V0, V1, V2);  // Sync pins 0, 1, and 2
   * @endcode
   */
  template<typename... Pins>
  void sync(Pins... pins) {
    uint8_t pinArray[] = {static_cast<uint8_t>(pins)...};
    for (size_t i = 0; i < sizeof...(pins); i++) {
      syncVirtual(pinArray[i]);
    }
  }
  
  // =========================================================================
  // EVENT HANDLERS (Manual Registration)
  // =========================================================================
  
  /**
   * @brief Register handler for virtual pin receive events (cloud → device)
   * @param pin Virtual pin number
   * @param handler Callback function
   * @note Consider using VWIRE_RECEIVE() macro for auto-registration
   */
  void onVirtualReceive(uint8_t pin, PinHandler handler);
  
  /**
   * @brief Register connection handler
   * @param handler Callback function
   * @note Consider using VWIRE_CONNECTED() macro for auto-registration
   */
  void onConnect(ConnectionHandler handler);
  
  /**
   * @brief Register disconnection handler
   * @param handler Callback function
   * @note Consider using VWIRE_DISCONNECTED() macro for auto-registration
   */
  void onDisconnect(ConnectionHandler handler);
  
  /**
   * @brief Register raw message handler
   * @param handler Callback function receiving topic and payload
   */
  void onMessage(RawMessageHandler handler);
  
  // =========================================================================
  // NOTIFICATIONS
  // =========================================================================
  
  /**
   * @brief Send push notification
   * @param message Notification text
   */
  void notify(const char* message);
  
  /**
   * @brief Send alarm notification (persistent with sound/vibration)
   * @param message Alarm message text
   * @note Requires PRO plan or higher. Free users are silently ignored.
   */
  void alarm(const char* message);
  
  /**
   * @brief Send alarm notification with custom sound
   * @param message Alarm message text
   * @param sound Sound name: "default", "urgent", or "warning"
   * @note Requires PRO plan or higher. Free users are silently ignored.
   */
  void alarm(const char* message, const char* sound);
  
  /**
   * @brief Send alarm notification with full options
   * @param message Alarm message text
   * @param sound Sound name: "default", "urgent", or "warning"
   * @param priority Priority level: 1=normal, 2=high, 3=critical
   * @note Requires PRO plan or higher. Free users are silently ignored.
   */
  void alarm(const char* message, const char* sound, uint8_t priority);

  /**
   * @brief Send alarm notification with full options including volume
   * @param message Alarm message text
   * @param sound Sound name: "default", "urgent", or "warning"
   * @param priority Priority level: 1=normal, 2=high, 3=critical
   * @param volume Volume level 0-100 (default: 50)
   * @note Requires PRO plan or higher. Free users are silently ignored.
   */
  void alarm(const char* message, const char* sound, uint8_t priority, uint8_t volume);
  
  /**
   * @brief Send email notification
   * @param subject Email subject
   * @param body Email body
   * @note Requires PRO plan or higher. Free users are silently ignored.
   */
  void email(const char* subject, const char* body);
  
  /**
   * @brief Send log message to server
   * @param message Log text
   */
  void log(const char* message);
  
  // =========================================================================
  // RELIABLE DELIVERY STATUS
  // =========================================================================
  
  /**
   * @brief Get number of messages awaiting acknowledgment
   * @return Pending message count
   */
  uint8_t getPendingCount();
  
  /**
   * @brief Check if any messages are awaiting acknowledgment
   * @return true if messages are pending
   */
  bool isDeliveryPending();
  
  // =========================================================================
  // DEVICE INFO
  // =========================================================================
  
  /**
   * @brief Get device identifier
   * @return Device ID string (same as auth token)
   */
  const char* getDeviceId();
  
  /**
   * @brief Get board name
   * @return Board name string (e.g., "ESP32", "ESP8266")
   */
  const char* getBoardName();
  
  /**
   * @brief Get library version
    * @return Version string (e.g., "2.0.0")
   */
  const char* getVersion();
  
  /**
   * @brief Get available heap memory
   * @return Free heap bytes (0 on unsupported platforms)
   */
  uint32_t getFreeHeap();
  
  /**
   * @brief Get uptime since connection
   * @return Seconds since begin() was called
   */
  uint32_t getUptime();
  
  // =========================================================================
  // OTA UPDATES (ESP32/ESP8266 only)
  // =========================================================================
  
  #if VWIRE_HAS_OTA
  /**
   * @brief Enable OTA (Over-The-Air) firmware updates
   * @param hostname mDNS hostname (optional, default: vwire-<deviceId>)
   * @param password OTA password (optional)
   */
  void enableOTA(const char* hostname = nullptr, const char* password = nullptr);
  
  /**
   * @brief Process OTA requests
   * @note Called automatically by run() when OTA is enabled
   */
  void handleOTA();
  #endif
  
  // =========================================================================
  // CLOUD OTA (ESP32/ESP8266 only)
  // =========================================================================
  
  #if VWIRE_ENABLE_CLOUD_OTA
  /**
   * @brief Enable Cloud OTA firmware updates from VWire server
   * 
   * When enabled, the device listens for OTA commands from the VWire platform
   * via MQTT and automatically downloads and installs firmware updates.
   * The device reports ota:true in its heartbeat so the server knows
   * cloud OTA is available.
   * 
   * @note Requires WiFi connection and MQTT connection to VWire server
   * @note Only ESP32 and ESP8266 boards support cloud OTA
   */
  void enableCloudOTA();

  /**
   * @brief Disable Cloud OTA firmware updates at runtime
   *
   * This turns off Cloud OTA handling for the current session. To strip the
   * feature from the binary entirely, use the compile-time disable macro.
   */
  void disableCloudOTA();
  
  /**
   * @brief Check if cloud OTA is enabled
   * @return true if cloud OTA is enabled
   */
  bool isCloudOTAEnabled();
  #endif

  // =========================================================================
  // GPIO CONVENIENCE API
  // =========================================================================

  /**
   * @brief Enable the built-in GPIO helper using the default addon instance
   * @return true if GPIO support is ready
   */
  bool enableGPIO();

  /**
   * @brief Attach a specific GPIO addon instance
   * @param gpio GPIO addon instance to use
   * @return true if attached, false if another GPIO addon is already active
   */
  bool useGPIO(VwireGPIO& gpio);

  /** @brief Check whether a GPIO helper is attached */
  bool isGPIOEnabled() const;

  /** @brief Add or update a GPIO pin by board label */
  bool addGPIOPin(const char* pinName, VwireGPIOMode mode,
                  uint16_t readInterval = 0);

  /** @brief Add or update a GPIO pin with explicit hardware GPIO number */
  bool addGPIOPin(const char* pinName, uint8_t gpioNumber,
                  VwireGPIOMode mode, uint16_t readInterval = 0);

  /** @brief Remove a managed GPIO pin */
  bool removeGPIOPin(const char* pinName);

  /** @brief Remove all managed GPIO pins */
  void clearGPIOPins();

  /** @brief Write a value to a managed GPIO pin */
  void gpioWrite(const char* pinName, int value);

  /** @brief Read the last known value of a managed GPIO pin */
  int gpioRead(const char* pinName) const;

  /** @brief Publish a GPIO value to the cloud */
  void gpioSend(const char* pinName, int value);

  /** @brief Get the number of managed GPIO pins */
  uint8_t getGPIOPinCount() const;
  
  // =========================================================================
  // ADDON SYSTEM
  // =========================================================================

  /**
   * @brief Register a modular addon (GPIO, etc.)
   *
   * The addon's onAttach() is called immediately.  Its other lifecycle
   * hooks are invoked automatically by begin()/run()/disconnect().
   *
   * @param addon  Reference to a VwireAddon subclass instance
   * @code
   * VwireGPIO gpio;
   * Vwire.addAddon(gpio);   // or: gpio.begin(Vwire);
   * @endcode
   */
  void addAddon(VwireAddon& addon);

  // =========================================================================
  // PUBLISH / SUBSCRIBE (for addons & advanced users)
  // =========================================================================

  /**
   * @brief Publish a raw MQTT message
   * @param topic   Full MQTT topic string
   * @param payload Null-terminated payload
   * @param retain  Retain flag (default: false)
   * @return true if published successfully
   */
  bool publish(const char* topic, const char* payload, bool retain = false);

  /**
   * @brief Subscribe to an MQTT topic
   * @param topic  Full MQTT topic string
   * @param qos    QoS level (default: 1)
   * @return true if subscribed successfully
   */
  bool subscribe(const char* topic, uint8_t qos = 1);

  // =========================================================================
  // LOG / DEBUG
  // =========================================================================

  /**
   * @brief Set callback for log output (recommended)
   *
   * All internal log messages are routed through this callback.
   * If no callback is set, log messages are discarded (zero overhead).
   *
   * @param cb  Function pointer receiving each log line
   * @code
   * Vwire.onLog([](const char* msg) { Serial.println(msg); });
   * @endcode
   */
  void onLog(VwireLogCallback cb);

  /**
   * @brief Route log output to an Arduino Stream
   * @param stream Destination stream, usually Serial
    * @note Logging is disabled by default until one of the log APIs is called.
   */
  void logTo(Stream& stream);

    /** @brief Disable callback and stream-based logging */
  void disableLog();

  /**
   * @brief Enable or disable debug output (legacy)
   * @param enable true to enable debug messages on debugStream
   */
  void setDebug(bool enable);
  
  /**
   * @brief Set debug output stream (legacy)
   * @param stream Stream to write debug output (default: Serial)
   */
  void setDebugStream(Stream& stream);
  
  /**
   * @brief Print comprehensive debug information
   */
  void printDebugInfo();
  
private:
  // =========================================================================
  // PRIVATE MEMBERS
  // =========================================================================
  
  // Settings and state
  VwireSettings _settings;              ///< Configuration settings
  VwireState _state;                    ///< Current connection state
  VwireError _lastError;                ///< Last error code
  char _deviceId[VWIRE_MAX_TOKEN_LENGTH]; ///< Device identifier
  char _hostname[33];                    ///< User-defined hostname (max 32 chars + null)
  bool _debug;                          ///< Debug output enabled (legacy)
  Stream* _debugStream;                 ///< Debug output stream (legacy)
  VwireLogCallback _logCallback;        ///< Log callback (preferred)
  unsigned long _startTime;             ///< Connection start time
  
  // Timing
  unsigned long _lastHeartbeat;         ///< Last heartbeat timestamp
  unsigned long _lastReconnectAttempt;  ///< Last reconnect attempt timestamp
  
  // Network clients (member variables for stable TLS)
  WiFiClient _wifiClient;               ///< Plain TCP client
  #if VWIRE_HAS_SSL
  WiFiClientSecure _secureClient;       ///< TLS/SSL client
  #endif
  PubSubClient _mqttClient;             ///< MQTT client
  
  // Handlers
  struct PinHandlerEntry {
    uint8_t pin;                        ///< Pin number
    PinHandler handler;                 ///< Handler function
    bool active;                        ///< Entry in use
  };
  PinHandlerEntry _pinHandlers[VWIRE_MAX_HANDLERS];  ///< Manual handler table
  int _pinHandlerCount;                              ///< Number of registered handlers
  
  ConnectionHandler _connectHandler;     ///< Manual connect handler
  ConnectionHandler _disconnectHandler;  ///< Manual disconnect handler
  RawMessageHandler _messageHandler;     ///< Raw message handler

  // Addon system
  VwireAddon* _addons[VWIRE_MAX_ADDONS]; ///< Registered addons
  uint8_t _addonCount;                   ///< Number of registered addons
  VwireGPIO* _gpioAddon;                 ///< Active GPIO addon, if attached
  VwireReliableDelivery* _reliableDeliveryAddon; ///< Reliable delivery module
  VwireOTAFeature* _otaFeature;          ///< OTA feature module
  
  // =========================================================================
  // PRIVATE METHODS
  // =========================================================================
  
  bool _connectWiFi(const char* ssid, const char* password);
  bool _connectMQTT();
  void _setupClient();
  void _handleMessage(char* topic, byte* payload, unsigned int length);
  static void _mqttCallbackWrapper(char* topic, byte* payload, unsigned int length);
  void _virtualSendInternal(uint8_t pin, const String& value);
  String _buildTopic(const char* type, int pin = -1);
  void _sendHeartbeat();
  void _setError(VwireError error);
  void _debugPrint(const char* message);
  void _debugPrintf(const char* format, ...);
  bool _ensureReliableDeliveryAddon();
  bool _ensureOTAFeature();

  friend class VwireReliableDelivery;
  friend class VwireOTAFeature;
  friend class VwireReliableDeliveryAddon;
  friend class VwireOTAAddon;
};

// =============================================================================
// GLOBAL INSTANCE
// =============================================================================
extern VwireClass Vwire;

#endif // VWIRE_H
