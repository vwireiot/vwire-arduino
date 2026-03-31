/*
 * Vwire IOT Arduino Library - GPIO Addon
 * 
 * Optional addon for direct digital and analog pin control from the VWire
 * cloud platform.  Include this header and instantiate VwireGPIO only when
 * you need cloud-managed GPIO pins.  If you don't include it, the linker
 * strips the entire module — zero flash cost.
 * 
 * Pin naming convention (matches cloud platform):
 *   D0–D99   → Digital pins (auto-resolved to board-specific GPIO)
 *             ESP8266/NodeMCU: D0→GPIO16, D1→GPIO5, D4→GPIO2, etc.
 *             ESP32/Others: Dx → GPIO x
 *   A0–A15   → Analog pins (auto-resolved via Arduino A0 mapping)
 * 
 * Usage:
 *   #include <Vwire.h>
 *   #include <VwireGPIO.h>
 *
 *   VwireGPIO gpio;
 *
 *   void setup() {
 *     Vwire.config(AUTH_TOKEN, DEVICE_ID);
 *     gpio.begin(Vwire);           // register as addon
 *     gpio.addPin("D4", VWIRE_GPIO_OUTPUT);  // optional manual pin
 *     Vwire.begin(WIFI_SSID, WIFI_PASS);
 *   }
 *   void loop() { Vwire.run(); }   // GPIO polling is automatic
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_GPIO_H
#define VWIRE_GPIO_H

#include <Arduino.h>
#include "VwireConfig.h"

// Forward declaration (full definition in Vwire.h)
class VwireClass;

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
 * Smart write behaviour (applies to OUTPUT and PWM modes):
 *   value 0   → digitalWrite(LOW)   — pin fully OFF
 *   value 1   → digitalWrite(HIGH)  — pin fully ON
 *   value 2-255 → analogWrite / PWM  — proportional duty cycle
 * The shared enum is declared in VwireConfig.h so sketches can use GPIO
 * helpers from Vwire.h without including this header.
 */

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
// VwireGPIO — STANDALONE ADDON
// =============================================================================

/**
 * @brief Cloud-managed GPIO pin addon for VWire
 *
 * Extends VwireAddon to receive MQTT pinconfig and Dx/Ax commands.
 * Polls input pins automatically during Vwire.run().
 *
 * @code
 * #include <Vwire.h>
 * #include <VwireGPIO.h>
 * VwireGPIO gpio;
 * void setup() {
 *   Vwire.config(TOKEN, DEVICE_ID);
 *   gpio.begin(Vwire);
 *   Vwire.begin(ssid, pass);
 * }
 * void loop() { Vwire.run(); }
 * @endcode
 */
class VwireGPIO : public VwireAddon {
public:
  VwireGPIO();

  /**
   * @brief Attach this addon to the Vwire core
   * @param vwire  Reference to the global VwireClass instance
   */
  void begin(VwireClass& vwire);

  // =========================================================================
  // PIN CONFIGURATION
  // =========================================================================

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
   * @param gpioNumber  Explicit physical GPIO number
   * @param mode  Pin mode
   * @param readInterval  Read interval for input modes (ms, 0 = use default)
   * @return true if added successfully
   */
  bool addPin(const char* pinName, uint8_t gpioNumber, VwireGPIOMode mode,
              uint16_t readInterval = 0);

  /**
   * @brief Remove a managed pin by name
   * @return true if found and removed
   */
  bool removePin(const char* pinName);

  /** @brief Remove all managed pins */
  void clearAll();

  // =========================================================================
  // PIN I/O
  // =========================================================================

  /**
   * @brief Write a value to a managed output pin
   * @param pinName Pin name (e.g., "D13")
   * @param value   0/1 for digital, 0-255 for PWM
   */
  void write(const char* pinName, int value);

  /**
   * @brief Read current cached value of a managed pin
   * @param pinName Pin name (e.g., "D2", "A0")
   * @return Last read value, or -1 if pin not managed
   */
  int read(const char* pinName) const;

  /**
   * @brief Publish a GPIO pin value to the cloud
   * @param pinName Pin name (e.g., "D5", "A0")
   * @param value   Value to publish
   */
  void send(const char* pinName, int value);

  // =========================================================================
  // QUERY
  // =========================================================================

  /** @brief Number of actively managed pins */
  uint8_t getPinCount() const;

  /** @brief Check if a pin name is managed */
  bool hasPin(const char* pinName) const;

  /** @brief Get current value of a managed pin (-1 if not found) */
  int getPinValue(const char* pinName) const;

  // =========================================================================
  // ADDON LIFECYCLE (called automatically by VwireClass)
  // =========================================================================
  void onAttach(VwireClass& vwire) override;
  void onConnect() override;
  bool onMessage(const char* topic, const char* payload) override;
  void onRun() override;

private:
  VwireClass* _vwire;

  // Pin storage
  VwireGPIOPin _pins[VWIRE_MAX_GPIO_PINS];
  uint8_t _count;

  int _findPin(const char* pinName) const;
  int _findFreeSlot() const;
  static VwireGPIOMode _parseMode(const char* modeStr);
  static uint8_t _resolvePinNumber(const char* pinName);
  void _applyHardwareMode(VwireGPIOPin& pin);
  int _readHardware(const VwireGPIOPin& pin) const;
  void _writeHardware(VwireGPIOPin& pin, int value);

  int _applyConfig(const char* jsonPayload);
  void _publishValue(const char* pinName, int value);
  bool handleCommand(const char* pinName, int value);
};

#endif // VWIRE_GPIO_H
