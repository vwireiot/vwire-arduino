/*
 * Vwire IOT Arduino Library - GPIO Pin Manager Implementation
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireGPIO.h"
#include <ArduinoJson.h>

// =============================================================================
// CONSTRUCTOR
// =============================================================================

VwireGPIOManager::VwireGPIOManager() : _count(0) {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    _pins[i].active = false;
    _pins[i].lastValue = -32768;  // sentinel — forces first publish
    _pins[i].lastRead = 0;
    _pins[i].flags = 0;
  }
}

// =============================================================================
// CONFIGURATION
// =============================================================================

int VwireGPIOManager::applyConfig(const char* jsonPayload) {
  // Parse JSON
  // Expected: {"pins":[{"pin":"D4","mode":"OUTPUT","interval":1000},...]}
  // GPIO number is auto-resolved from pin name using platform-specific mapping.
  StaticJsonDocument<VWIRE_JSON_BUFFER_SIZE> doc;
  DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    return -1;  // Parse error
  }

  JsonArray pinsArr = doc["pins"].as<JsonArray>();
  if (pinsArr.isNull()) {
    return -1;  // No pins array
  }

  int configured = 0;

  for (JsonObject pinObj : pinsArr) {
    const char* pinName = pinObj["pin"];
    if (!pinName) continue;

    const char* modeStr = pinObj["mode"];
    if (!modeStr) continue;

    uint16_t interval = pinObj["interval"] | 0;

    VwireGPIOMode mode = _parseMode(modeStr);
    if (mode == VWIRE_GPIO_DISABLED) continue;  // Unknown mode string

    if (addPin(pinName, mode, interval)) {
      configured++;
    }
  }

  return configured;
}

bool VwireGPIOManager::addPin(const char* pinName, VwireGPIOMode mode,
                               uint16_t readInterval) {
  uint8_t gpio = _resolvePinNumber(pinName);
  if (gpio == 255) return false;  // Unresolvable pin name
  return addPin(pinName, gpio, mode, readInterval);
}

bool VwireGPIOManager::addPin(const char* pinName, uint8_t gpioNumber,
                               VwireGPIOMode mode, uint16_t readInterval) {
  // Check if pin already exists — update in place
  int idx = _findPin(pinName);
  if (idx < 0) {
    idx = _findFreeSlot();
    if (idx < 0) return false;  // Table full
    _count++;
  }

  VwireGPIOPin& p = _pins[idx];
  strncpy(p.pinName, pinName, sizeof(p.pinName) - 1);
  p.pinName[sizeof(p.pinName) - 1] = '\0';
  // Uppercase the pin name for consistency
  for (char* c = p.pinName; *c; c++) *c = toupper(*c);

  p.gpioNumber = gpioNumber;
  p.mode = mode;
  p.active = true;
  p.lastValue = -32768;  // Force first publish
  p.lastRead = 0;
  p.flags = 0;           // Reset runtime flags (PWM init, etc.)

  // Clamp read interval
  if (readInterval == 0) {
    p.readInterval = VWIRE_GPIO_READ_INTERVAL;
  } else if (readInterval < VWIRE_GPIO_MIN_READ_INTERVAL) {
    p.readInterval = VWIRE_GPIO_MIN_READ_INTERVAL;
  } else if (readInterval > VWIRE_GPIO_MAX_READ_INTERVAL) {
    p.readInterval = VWIRE_GPIO_MAX_READ_INTERVAL;
  } else {
    p.readInterval = readInterval;
  }

  // Apply hardware mode
  _applyHardwareMode(p);

  return true;
}

bool VwireGPIOManager::removePin(const char* pinName) {
  int idx = _findPin(pinName);
  if (idx < 0) return false;

  _pins[idx].active = false;
  _count--;
  return true;
}

void VwireGPIOManager::clearAll() {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    _pins[i].active = false;
  }
  _count = 0;
}

// =============================================================================
// RUNTIME
// =============================================================================

void VwireGPIOManager::poll(PublishGPIOFn publishFn) {
  if (_count == 0 || !publishFn) return;

  unsigned long now = millis();

  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    VwireGPIOPin& p = _pins[i];
    if (!p.active) continue;

    // Only poll input pins
    bool isInput = (p.mode == VWIRE_GPIO_INPUT ||
                    p.mode == VWIRE_GPIO_INPUT_PULLUP ||
                    p.mode == VWIRE_GPIO_ANALOG_INPUT);
    if (!isInput) continue;

    // Check if it's time to read
    if (now - p.lastRead < p.readInterval) continue;
    p.lastRead = now;

    int value = _readHardware(p);

    // Only publish if value changed (or first read)
    if (value != p.lastValue) {
      p.lastValue = value;
      publishFn(p.pinName, value);
    }
  }
}

bool VwireGPIOManager::handleCommand(const char* pinName, int value) {
  int idx = _findPin(pinName);
  if (idx < 0) return false;

  VwireGPIOPin& p = _pins[idx];

  // Only write to output pins
  if (p.mode != VWIRE_GPIO_OUTPUT && p.mode != VWIRE_GPIO_PWM) {
    return false;
  }

  _writeHardware(p, value);
  p.lastValue = value;
  return true;
}

// =============================================================================
// QUERY
// =============================================================================

uint8_t VwireGPIOManager::getPinCount() const { return _count; }

bool VwireGPIOManager::hasPin(const char* pinName) const {
  return _findPin(pinName) >= 0;
}

int VwireGPIOManager::getPinValue(const char* pinName) const {
  int idx = _findPin(pinName);
  if (idx < 0) return -1;
  return _pins[idx].lastValue;
}

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

int VwireGPIOManager::_findPin(const char* pinName) const {
  // Case-insensitive compare
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    if (!_pins[i].active) continue;
    // Simple case-insensitive pin name comparison
    const char* a = _pins[i].pinName;
    const char* b = pinName;
    bool match = true;
    while (*a && *b) {
      if (toupper(*a) != toupper(*b)) { match = false; break; }
      a++; b++;
    }
    if (match && *a == '\0' && *b == '\0') return i;
  }
  return -1;
}

int VwireGPIOManager::_findFreeSlot() const {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    if (!_pins[i].active) return i;
  }
  return -1;
}

VwireGPIOMode VwireGPIOManager::_parseMode(const char* modeStr) {
  if (!modeStr) return VWIRE_GPIO_DISABLED;

  // Case-insensitive comparison
  if (strcasecmp(modeStr, "OUTPUT") == 0)       return VWIRE_GPIO_OUTPUT;
  if (strcasecmp(modeStr, "INPUT") == 0)        return VWIRE_GPIO_INPUT;
  if (strcasecmp(modeStr, "INPUT_PULLUP") == 0) return VWIRE_GPIO_INPUT_PULLUP;
  if (strcasecmp(modeStr, "PWM") == 0)          return VWIRE_GPIO_PWM;
  if (strcasecmp(modeStr, "ANALOG_INPUT") == 0) return VWIRE_GPIO_ANALOG_INPUT;

  return VWIRE_GPIO_DISABLED;
}

uint8_t VwireGPIOManager::_resolvePinNumber(const char* pinName) {
  if (!pinName || pinName[0] == '\0') return 255;

  char prefix = toupper(pinName[0]);
  int num = atoi(pinName + 1);

  if (prefix == 'D') {
    // ---------------------------------------------------------------
    // Pin name → GPIO resolution strategy:
    //
    // ESP8266 / NodeMCU is the ONLY mainstream platform where Dx ≠ GPIO x
    // (D4 = GPIO 2, D2 = GPIO 4, etc.).  The Arduino ESP8266 core
    // provides D0–D10 as `static const uint8_t` in pins_arduino.h —
    // they are ALWAYS available when compiling for ESP8266, so we use
    // them directly (no #ifdef per pin — that fails for static const).
    //
    // On every other platform (ESP32, RP2040, SAMD, AVR, STM32…)
    // the convention is Dx == GPIO x, so we just return the number.
    //
    // Adding a new non-trivial board in the future:
    //   1. Add a new #elif defined(VWIRE_BOARD_xxx) block
    //   2. Use that board's core-provided pin constants
    // ---------------------------------------------------------------
    #if defined(VWIRE_BOARD_ESP8266)
      switch (num) {
        case 0:  return D0;   // GPIO 16
        case 1:  return D1;   // GPIO 5
        case 2:  return D2;   // GPIO 4
        case 3:  return D3;   // GPIO 0
        case 4:  return D4;   // GPIO 2  (built-in LED on most NodeMCU)
        case 5:  return D5;   // GPIO 14
        case 6:  return D6;   // GPIO 12
        case 7:  return D7;   // GPIO 13
        case 8:  return D8;   // GPIO 15
        case 9:  return D9;   // GPIO 3  (RX)
        case 10: return D10;  // GPIO 1  (TX)
        default: return (uint8_t)num;
      }
    #else
      // ESP32, RP2040, AVR, SAMD, STM32, etc.: Dx == GPIO x
      return (uint8_t)num;
    #endif
  }
  else if (prefix == 'A') {
    // Analog pin mapping.
    // On ESP8266 A0 = GPIO 17 (the only ADC pin).
    // On ESP32 analog pins vary; Ax → GPIO x is a reasonable default.
    // On standard Arduino, A0 is a const/macro (14 on Uno, etc.).
    #if defined(VWIRE_BOARD_ESP8266)
      // ESP8266 has only one analog pin: A0 → GPIO 17 (TOUT)
      return 17;
    #elif defined(A0)
      return (uint8_t)(A0 + num);
    #else
      return (uint8_t)num;
    #endif
  }

  return 255;  // Unknown prefix
}

void VwireGPIOManager::_applyHardwareMode(VwireGPIOPin& pin) {
  switch (pin.mode) {
    case VWIRE_GPIO_OUTPUT:
    case VWIRE_GPIO_PWM:
      pinMode(pin.gpioNumber, OUTPUT);
      break;
    case VWIRE_GPIO_INPUT:
      pinMode(pin.gpioNumber, INPUT);
      break;
    case VWIRE_GPIO_INPUT_PULLUP:
      pinMode(pin.gpioNumber, INPUT_PULLUP);
      break;
    case VWIRE_GPIO_ANALOG_INPUT:
      // Most platforms don't require explicit pinMode for analog input
      // but some boards benefit from it
      #if defined(VWIRE_BOARD_ESP32)
        // ESP32: analogRead works without explicit mode on ADC pins
      #else
        pinMode(pin.gpioNumber, INPUT);
      #endif
      break;
    default:
      break;
  }
}

int VwireGPIOManager::_readHardware(const VwireGPIOPin& pin) const {
  if (pin.mode == VWIRE_GPIO_ANALOG_INPUT) {
    return analogRead(pin.gpioNumber);
  }
  // Digital input
  return digitalRead(pin.gpioNumber);
}

void VwireGPIOManager::_writeHardware(VwireGPIOPin& pin, int value) {
  // =========================================================================
  // Smart write (Blynk-style):
  //   0 or 1   → digitalWrite  (clean digital HIGH / LOW)
  //   2 – 255  → analogWrite   (PWM duty cycle, platform-specific)
  //
  // This lets a switch widget (sends 0/1) and a slider widget (sends 0-255)
  // both work on the same OUTPUT pin without the user changing any config.
  // =========================================================================

  value = constrain(value, 0, 255);

  if (value <= 1) {
    // ---- Digital ON / OFF ------------------------------------------------
    #if defined(VWIRE_BOARD_ESP32)
      #if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
        // ESP32 Core 2.x: if a ledc channel was previously attached to this
        // pin we must detach it first so that digitalWrite() works again.
        if (pin.flags & VWIRE_GPIO_FLAG_PWM_INIT) {
          ledcDetachPin(pin.gpioNumber);
          pinMode(pin.gpioNumber, OUTPUT);
          pin.flags &= ~VWIRE_GPIO_FLAG_PWM_INIT;
        }
      #endif
    #endif
    digitalWrite(pin.gpioNumber, value ? HIGH : LOW);
    return;
  }

  // ---- PWM (value 2 – 255) -----------------------------------------------
  #if defined(VWIRE_BOARD_ESP32)
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      // ESP32 Core 3.x — native analogWrite (8-bit, 0-255)
      analogWrite(pin.gpioNumber, value);
    #else
      // ESP32 Core 2.x — ledc API
      uint8_t ch = (uint8_t)(&pin - _pins);   // channel = slot index
      if (!(pin.flags & VWIRE_GPIO_FLAG_PWM_INIT)) {
        ledcSetup(ch, 5000, 8);               // 5 kHz, 8-bit resolution
        ledcAttachPin(pin.gpioNumber, ch);
        pin.flags |= VWIRE_GPIO_FLAG_PWM_INIT;
      }
      ledcWrite(ch, value);
    #endif
  #elif defined(VWIRE_BOARD_ESP8266)
    // ESP8266 default PWM range is 0-PWMRANGE (typically 1023).
    // Scale our 8-bit input to the full hardware range.
    analogWrite(pin.gpioNumber, (uint8_t)value);
  #else
    // Standard Arduino (Uno, Mega, Nano, etc.)
    analogWrite(pin.gpioNumber, value);
  #endif
}
