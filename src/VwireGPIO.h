/*
 * Vwire IOT Arduino Library - GPIO Pin Manager
 * 
 * Handles direct digital and analog pin control from the VWire cloud platform.
 * Supports auto-read of inputs, command-driven writes, and runtime pin
 * configuration via MQTT pinconfig messages.
 * 
 * Pin naming convention (matches cloud platform):
 *   D0–D99   → Digital pins (auto-resolved to board-specific GPIO)
 *             ESP8266/NodeMCU: D0→GPIO16, D1→GPIO5, D4→GPIO2, etc.
 *             ESP32/Others: Dx → GPIO x
 *   A0–A15   → Analog pins (auto-resolved via Arduino A0 mapping)
 *   V0–V255  → Virtual pins (handled by existing VirtualPin system)
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_GPIO_H
#define VWIRE_GPIO_H

#include <Arduino.h>
#include "VwireConfig.h"

// =============================================================================
// GPIO CONFIGURATION DEFAULTS
// =============================================================================

/** @brief Maximum number of GPIO pins that can be managed simultaneously */
#ifndef VWIRE_MAX_GPIO_PINS
  #if defined(VWIRE_BOARD_ESP32)
    #define VWIRE_MAX_GPIO_PINS 24
  #elif defined(VWIRE_BOARD_ESP8266)
    #define VWIRE_MAX_GPIO_PINS 12
  #else
    #define VWIRE_MAX_GPIO_PINS 16
  #endif
#endif

/** @brief Default interval for polling input pins (ms) */
#ifndef VWIRE_GPIO_READ_INTERVAL
  #define VWIRE_GPIO_READ_INTERVAL 1000
#endif

/** @brief Minimum allowed read interval (ms) */
#define VWIRE_GPIO_MIN_READ_INTERVAL 100

/** @brief Maximum allowed read interval (ms) */
#define VWIRE_GPIO_MAX_READ_INTERVAL 60000

// =============================================================================
// GPIO PIN MODES (match server-side PinMode enum)
// =============================================================================

/**
 * @brief Pin mode types matching the cloud platform definition.
 * String values used in MQTT JSON: "OUTPUT", "INPUT", "INPUT_PULLUP", "PWM", "ANALOG_INPUT"
 *
 * Smart write behaviour (Blynk-style, applies to OUTPUT and PWM modes):
 *   value 0   → digitalWrite(LOW)   — pin fully OFF
 *   value 1   → digitalWrite(HIGH)  — pin fully ON
 *   value 2-255 → analogWrite / PWM  — proportional duty cycle
 * The library handles all platform differences (ESP32 ledc, ESP8266
 * PWMRANGE scaling, standard Arduino analogWrite) automatically.
 */
typedef enum {
  VWIRE_GPIO_OUTPUT       = 0,   ///< Digital/PWM output (auto: 0-1 digital, 2-255 PWM)
  VWIRE_GPIO_INPUT        = 1,   ///< Digital input (floating)
  VWIRE_GPIO_INPUT_PULLUP = 2,   ///< Digital input with internal pull-up
  VWIRE_GPIO_PWM          = 3,   ///< PWM output (alias for OUTPUT, kept for compat)
  VWIRE_GPIO_ANALOG_INPUT = 4,   ///< Analog input (ADC reading)
  VWIRE_GPIO_DISABLED     = 255  ///< Pin not managed
} VwireGPIOMode;

// =============================================================================
// GPIO PIN FLAGS
// =============================================================================

#define VWIRE_GPIO_FLAG_PWM_INIT  0x01  ///< PWM channel has been set up (ESP32 Core 2.x)

// =============================================================================
// GPIO PIN ENTRY
// =============================================================================

/**
 * @brief Represents a single managed GPIO pin
 */
struct VwireGPIOPin {
  char   pinName[6];        ///< Cloud pin name (e.g., "D2", "A0")
  uint8_t gpioNumber;       ///< Resolved hardware GPIO number
  VwireGPIOMode mode;       ///< Configured mode
  uint16_t readInterval;    ///< Read interval for input pins (ms)
  unsigned long lastRead;   ///< Timestamp of last read
  int16_t lastValue;        ///< Last read/written value (for change detection)
  bool active;              ///< Whether this slot is in use
  uint8_t flags;            ///< Runtime flags (VWIRE_GPIO_FLAG_*)
};

// =============================================================================
// GPIO MANAGER CLASS
// =============================================================================

/**
 * @brief Manages direct GPIO pin control for the VWire platform
 * 
 * The GPIO manager processes pinconfig messages from the server, sets up
 * hardware pin modes, automatically reads input pins at configured intervals,
 * and applies command values to output pins.
 * 
 * @code
 * // In sketch — just enable GPIO; everything else is automatic:
 * Vwire.enableGPIO();
 * 
 * // Optional: manually add a pin without waiting for cloud config
 * Vwire.gpioWrite("D13", HIGH);
 * @endcode
 */
class VwireGPIOManager {
public:
  /** @brief Constructor — initializes all slots as inactive */
  VwireGPIOManager();

  // =========================================================================
  // CONFIGURATION
  // =========================================================================

  /**
   * @brief Apply a pinconfig JSON payload from the server.
   * Expected format:
   *   {"pins":[{"pin":"D4","mode":"OUTPUT"},
   *            {"pin":"A0","mode":"ANALOG_INPUT","interval":500}]}
   *
   * The hardware GPIO number is automatically resolved from the pin name
   * using platform-specific mapping (e.g., D4→GPIO2 on ESP8266).
   *
   * @param jsonPayload  Null-terminated JSON string
   * @return Number of pins successfully configured
   */
  int applyConfig(const char* jsonPayload);

  /**
   * @brief Add or update a GPIO pin (auto-resolves hardware GPIO from name)
   * @param pinName  Cloud pin name (e.g., "D5", "A0")
   * @param mode  Pin mode
   * @param readInterval  Read interval for input modes (ms, 0 = use default)
   * @return true if added successfully
   */
  bool addPin(const char* pinName, VwireGPIOMode mode,
              uint16_t readInterval = 0);

  /**
   * @brief Add or update a GPIO pin with explicit hardware GPIO number
   * @param pinName  Cloud pin name (e.g., "D5", "A0")
   * @param gpioNumber  Explicit physical GPIO number (overrides auto-resolve)
   * @param mode  Pin mode
   * @param readInterval  Read interval for input modes (ms, 0 = use default)
   * @return true if added successfully
   */
  bool addPin(const char* pinName, uint8_t gpioNumber, VwireGPIOMode mode,
              uint16_t readInterval = 0);

  /**
   * @brief Remove a managed pin by name
   * @param pinName  Cloud pin name (e.g., "D5")
   * @return true if found and removed
   */
  bool removePin(const char* pinName);

  /**
   * @brief Remove all managed pins
   */
  void clearAll();

  // =========================================================================
  // RUNTIME
  // =========================================================================

  /**
   * @brief Poll input pins and publish changed values.
   * Call this from the main loop (Vwire.run() calls it automatically).
   *
   * @param publishFn  Function pointer that publishes a value to the cloud.
   *                   Signature: void fn(const char* pinName, int value)
   */
  typedef void (*PublishGPIOFn)(const char* pinName, int value);
  void poll(PublishGPIOFn publishFn);

  /**
   * @brief Handle an incoming command for a GPIO pin.
   * Called when a message arrives on vwire/{id}/cmd/D* or /cmd/A*.
   *
   * @param pinName  Pin name from the topic (e.g., "D13")
   * @param value    Value from the payload (e.g., "1", "128")
   * @return true if the pin was found and written
   */
  bool handleCommand(const char* pinName, int value);

  // =========================================================================
  // QUERY
  // =========================================================================

  /** @brief Get number of actively managed pins */
  uint8_t getPinCount() const;

  /** @brief Check if a pin name is managed */
  bool hasPin(const char* pinName) const;

  /** @brief Get current value of a managed pin (-1 if not found) */
  int getPinValue(const char* pinName) const;

private:
  VwireGPIOPin _pins[VWIRE_MAX_GPIO_PINS];
  uint8_t _count;

  /** @brief Find slot index by name (-1 if not found) */
  int _findPin(const char* pinName) const;

  /** @brief Find first empty slot (-1 if full) */
  int _findFreeSlot() const;

  /** @brief Convert mode string ("OUTPUT", etc.) to enum */
  static VwireGPIOMode _parseMode(const char* modeStr);

  /**
   * @brief Resolve pin name to hardware GPIO number.
   * Uses platform-specific mapping:
   *   ESP8266: D0-D10 map to NodeMCU GPIO numbers (D4→GPIO2, etc.)
   *   ESP32/Others: Dx → x, Ax → analogInputToDigitalPin(x)
   * @param pinName  Pin name (e.g., "D4", "A0")
   * @return Hardware GPIO number, or 255 if unresolvable
   */
  static uint8_t _resolvePinNumber(const char* pinName);

  /** @brief Apply Arduino pinMode() for a slot */
  void _applyHardwareMode(VwireGPIOPin& pin);

  /** @brief Read a pin's current hardware value */
  int _readHardware(const VwireGPIOPin& pin) const;

  /** @brief Write a value to a pin's hardware */
  void _writeHardware(VwireGPIOPin& pin, int value);
};

#endif // VWIRE_GPIO_H
