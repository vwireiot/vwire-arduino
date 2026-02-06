/*
 * Vwire IOT Arduino Library - Configuration
 * 
 * Board detection, platform-specific settings, and default values.
 * This file is automatically included by Vwire.h.
 * 
 * Supported Platforms:
 * - ESP32 (all variants)
 * - ESP8266 (NodeMCU, Wemos D1, etc.)
 * - RP2040 (Raspberry Pi Pico W)
 * - SAMD (Arduino MKR, Zero)
 * - Generic Arduino with WiFi
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_CONFIG_H
#define VWIRE_CONFIG_H

// =============================================================================
// VERSION
// =============================================================================

/** @brief Library version string */
#define VWIRE_VERSION "3.1.0"

// =============================================================================
// BOARD DETECTION
// =============================================================================
/**
 * @brief Automatic board detection and capability flags
 * 
 * Defines platform-specific capabilities:
 * - VWIRE_HAS_WIFI: WiFi support available
 * - VWIRE_HAS_SSL: TLS/SSL support available
 * - VWIRE_HAS_OTA: Over-the-air updates supported
 * - VWIRE_HAS_DEEP_SLEEP: Deep sleep mode available
 */

#if defined(ESP32)
  #define VWIRE_BOARD_ESP32
  #define VWIRE_BOARD_NAME "ESP32"
  #define VWIRE_HAS_WIFI 1
  #define VWIRE_HAS_SSL 1
  #define VWIRE_HAS_OTA 1
  #define VWIRE_HAS_DEEP_SLEEP 1
  #define VWIRE_MAX_PAYLOAD_LENGTH 2048   ///< Maximum MQTT payload size
  #define VWIRE_JSON_BUFFER_SIZE 1024     ///< JSON parsing buffer size

#elif defined(ESP8266)
  #define VWIRE_BOARD_ESP8266
  #define VWIRE_BOARD_NAME "ESP8266"
  #define VWIRE_HAS_WIFI 1
  #define VWIRE_HAS_SSL 1
  #define VWIRE_HAS_OTA 1
  #define VWIRE_HAS_DEEP_SLEEP 1
  #define VWIRE_MAX_PAYLOAD_LENGTH 1024   ///< Limited by ESP8266 RAM
  #define VWIRE_JSON_BUFFER_SIZE 512      ///< Smaller buffer for ESP8266

#elif defined(ARDUINO_ARCH_RP2040)
  #define VWIRE_BOARD_RP2040
  #define VWIRE_BOARD_NAME "RP2040"
  #define VWIRE_HAS_WIFI 1
  #define VWIRE_HAS_SSL 0                 ///< Limited SSL support on RP2040
  #define VWIRE_HAS_OTA 0
  #define VWIRE_HAS_DEEP_SLEEP 1
  #define VWIRE_MAX_PAYLOAD_LENGTH 1024
  #define VWIRE_JSON_BUFFER_SIZE 512

#elif defined(ARDUINO_ARCH_SAMD)
  #define VWIRE_BOARD_SAMD
  #define VWIRE_BOARD_NAME "SAMD"
  #define VWIRE_HAS_WIFI 1
  #define VWIRE_HAS_SSL 0
  #define VWIRE_HAS_OTA 0
  #define VWIRE_HAS_DEEP_SLEEP 0
  #define VWIRE_MAX_PAYLOAD_LENGTH 512
  #define VWIRE_JSON_BUFFER_SIZE 256

#else
  #define VWIRE_BOARD_GENERIC
  #define VWIRE_BOARD_NAME "Generic"
  #define VWIRE_HAS_WIFI 1
  #define VWIRE_HAS_SSL 0
  #define VWIRE_HAS_OTA 0
  #define VWIRE_HAS_DEEP_SLEEP 0
  #define VWIRE_MAX_PAYLOAD_LENGTH 512
  #define VWIRE_JSON_BUFFER_SIZE 256
#endif

// =============================================================================
// DEFAULT SERVER CONFIGURATION
// =============================================================================

/** @brief Default Vwire IOT cloud server */
#define VWIRE_DEFAULT_SERVER "mqtt.vwire.io"

/** @brief Default port for plain MQTT (TCP) */
#define VWIRE_DEFAULT_PORT_TCP 1883

/** @brief Default port for MQTT over TLS - RECOMMENDED */
#define VWIRE_DEFAULT_PORT_TLS 8883

// =============================================================================
// TRANSPORT TYPES
// =============================================================================

/**
 * @brief Transport protocol for MQTT connection
 */
typedef enum {
  VWIRE_TRANSPORT_TCP = 0,       ///< Plain MQTT over TCP (port 1883)
  VWIRE_TRANSPORT_TCP_SSL = 1    ///< MQTT over TLS (port 8883) - RECOMMENDED
} VwireTransport;

// =============================================================================
// VIRTUAL PIN LIMITS
// =============================================================================

/** @brief Maximum virtual pin number (0-127 supported) */
#define VWIRE_MAX_VIRTUAL_PINS 128

/** @brief Maximum number of manually registered handlers */
#define VWIRE_MAX_HANDLERS 32

/** @brief Maximum auth token length */
#define VWIRE_MAX_TOKEN_LENGTH 64

/** @brief Maximum server hostname length */
#define VWIRE_MAX_SERVER_LENGTH 64

// =============================================================================
// TIMING CONFIGURATION
// =============================================================================

/** @brief Default heartbeat interval (30 seconds) */
#define VWIRE_DEFAULT_HEARTBEAT_INTERVAL 30000

/** @brief Default reconnect attempt interval (5 seconds) */
#define VWIRE_DEFAULT_RECONNECT_INTERVAL 5000

/** @brief Default WiFi connection timeout (30 seconds) */
#define VWIRE_DEFAULT_WIFI_TIMEOUT 30000

/** @brief Default MQTT connection timeout (10 seconds) */
#define VWIRE_DEFAULT_MQTT_TIMEOUT 10000

// =============================================================================
// RELIABLE DELIVERY CONFIGURATION
// =============================================================================

/** @brief Default ACK timeout before retry (5 seconds) */
#define VWIRE_DEFAULT_ACK_TIMEOUT 5000

/** @brief Default maximum retry attempts */
#define VWIRE_DEFAULT_MAX_RETRIES 3

/** @brief Maximum queued messages (memory constraint) */
#define VWIRE_MAX_PENDING_MESSAGES 10

// =============================================================================
// CONNECTION STATES
// =============================================================================

/**
 * @brief Vwire connection state machine states
 */
typedef enum {
  VWIRE_STATE_IDLE = 0,          ///< Not started
  VWIRE_STATE_CONNECTING_WIFI,   ///< Connecting to WiFi
  VWIRE_STATE_CONNECTING_MQTT,   ///< WiFi connected, connecting to MQTT
  VWIRE_STATE_CONNECTED,         ///< Fully connected
  VWIRE_STATE_DISCONNECTED,      ///< Was connected, now disconnected
  VWIRE_STATE_ERROR              ///< Error state
} VwireState;

// =============================================================================
// ERROR CODES
// =============================================================================

/**
 * @brief Vwire error codes
 */
typedef enum {
  VWIRE_ERR_NONE = 0,            ///< No error
  VWIRE_ERR_NO_TOKEN,            ///< Auth token not configured
  VWIRE_ERR_WIFI_FAILED,         ///< WiFi connection failed
  VWIRE_ERR_MQTT_FAILED,         ///< MQTT connection failed
  VWIRE_ERR_NOT_CONNECTED,       ///< Not connected (operation requires connection)
  VWIRE_ERR_INVALID_PIN,         ///< Invalid virtual pin number
  VWIRE_ERR_BUFFER_FULL,         ///< Buffer overflow
  VWIRE_ERR_HANDLER_FULL,        ///< Maximum handlers reached
  VWIRE_ERR_TIMEOUT,             ///< Operation timed out
  VWIRE_ERR_SSL_FAILED,          ///< TLS/SSL connection failed
  VWIRE_ERR_QUEUE_FULL           ///< Reliable delivery queue full
} VwireError;

#endif // VWIRE_CONFIG_H
