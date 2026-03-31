/*
 * Vwire IOT Arduino Library - GPIO Addon Implementation
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireGPIO.h"
#include "Vwire.h"        // Full VwireClass definition (needed for publish/subscribe)
#include <ArduinoJson.h>

#if VWIRE_ENABLE_LOGGING
  #define VWIRE_LOG(message) _vwire->_debugPrint(message)
  #define VWIRE_LOGF(...) _vwire->_debugPrintf(__VA_ARGS__)
#else
  #define VWIRE_LOG(message) do { } while (0)
  #define VWIRE_LOGF(...) do { } while (0)
#endif

#if VWIRE_ENABLE_GPIO

static VwireGPIO _vwireDefaultGPIOAddon;

bool VwireClass::enableGPIO() {
  return useGPIO(_vwireDefaultGPIOAddon);
}

bool VwireClass::useGPIO(VwireGPIO& gpio) {
  if (_gpioAddon == &gpio) {
    return true;
  }

  if (_gpioAddon && _gpioAddon != &gpio) {
    _debugPrint("[Vwire] Error: GPIO addon already attached");
    return false;
  }

  _gpioAddon = &gpio;
  gpio.begin(*this);
  return true;
}

bool VwireClass::isGPIOEnabled() const {
  return _gpioAddon != nullptr;
}

bool VwireClass::addGPIOPin(const char* pinName, VwireGPIOMode mode,
                            uint16_t readInterval) {
  if (!enableGPIO()) return false;
  return _gpioAddon->addPin(pinName, mode, readInterval);
}

bool VwireClass::addGPIOPin(const char* pinName, uint8_t gpioNumber,
                            VwireGPIOMode mode, uint16_t readInterval) {
  if (!enableGPIO()) return false;
  return _gpioAddon->addPin(pinName, gpioNumber, mode, readInterval);
}

bool VwireClass::removeGPIOPin(const char* pinName) {
  return _gpioAddon ? _gpioAddon->removePin(pinName) : false;
}

void VwireClass::clearGPIOPins() {
  if (_gpioAddon) _gpioAddon->clearAll();
}

void VwireClass::gpioWrite(const char* pinName, int value) {
  if (_gpioAddon) _gpioAddon->write(pinName, value);
}

int VwireClass::gpioRead(const char* pinName) const {
  return _gpioAddon ? _gpioAddon->getPinValue(pinName) : -1;
}

void VwireClass::gpioSend(const char* pinName, int value) {
  if (_gpioAddon) _gpioAddon->send(pinName, value);
}

uint8_t VwireClass::getGPIOPinCount() const {
  return _gpioAddon ? _gpioAddon->getPinCount() : 0;
}

// =============================================================================
// CONSTRUCTOR
// =============================================================================

VwireGPIO::VwireGPIO() : _vwire(nullptr), _count(0) {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    _pins[i].active = false;
    _pins[i].lastValue = -32768;  // sentinel — forces first publish
    _pins[i].lastRead = 0;
    _pins[i].flags = 0;
  }
}

// =============================================================================
// BEGIN (user-facing entry point)
// =============================================================================

void VwireGPIO::begin(VwireClass& vwire) {
  _vwire = &vwire;
  vwire.addAddon(*this);
}

// =============================================================================
// ADDON LIFECYCLE
// =============================================================================

void VwireGPIO::onAttach(VwireClass& vwire) {
  _vwire = &vwire;
}

void VwireGPIO::onConnect() {
  if (!_vwire) return;

  // Subscribe to pinconfig topic: vwire/{deviceId}/pinconfig
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/pinconfig", _vwire->getDeviceId());
  _vwire->subscribe(topic, 1);
}

bool VwireGPIO::onMessage(const char* topic, const char* payload) {
  // Handle pinconfig: vwire/{deviceId}/pinconfig
  char* pinconfigPos = strstr(topic, "/pinconfig");
  if (pinconfigPos && *(pinconfigPos + 10) == '\0') {
    _applyConfig(payload);
    return true;
  }

  // Handle D*/A* commands: vwire/{deviceId}/cmd/D* or /cmd/A*
  char* cmdPos = strstr(topic, "/cmd/");
  if (cmdPos) {
    char* pinStr = cmdPos + 5;
    if (*pinStr == 'D' || *pinStr == 'd' || *pinStr == 'A' || *pinStr == 'a') {
      char gpioName[6];
      strncpy(gpioName, pinStr, sizeof(gpioName) - 1);
      gpioName[sizeof(gpioName) - 1] = '\0';
      for (char* c = gpioName; *c; c++) *c = toupper(*c);

      int value = atoi(payload);
      handleCommand(gpioName, value);
      return true;
    }
  }

  return false;  // Not handled
}

void VwireGPIO::onRun() {
  if (_count == 0 || !_vwire) return;

  unsigned long now = millis();

  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    VwireGPIOPin& p = _pins[i];
    if (!p.active) continue;

    bool isInput = (p.mode == VWIRE_GPIO_INPUT ||
                    p.mode == VWIRE_GPIO_INPUT_PULLUP ||
                    p.mode == VWIRE_GPIO_ANALOG_INPUT);
    if (!isInput) continue;

    if (now - p.lastRead < p.readInterval) continue;
    p.lastRead = now;

    int value = _readHardware(p);

    if (value != p.lastValue) {
      p.lastValue = value;
      _publishValue(p.pinName, value);
    }
  }
}

// =============================================================================
// PIN CONFIGURATION
// =============================================================================

int VwireGPIO::_applyConfig(const char* jsonPayload) {
  StaticJsonDocument<VWIRE_JSON_BUFFER_SIZE> doc;
  DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) return -1;

  JsonArray pinsArr = doc["pins"].as<JsonArray>();
  if (pinsArr.isNull()) return -1;

  int configured = 0;
  for (JsonObject pinObj : pinsArr) {
    const char* pinName = pinObj["pin"];
    if (!pinName) continue;
    const char* modeStr = pinObj["mode"];
    if (!modeStr) continue;
    uint16_t interval = pinObj["interval"] | 0;
    VwireGPIOMode mode = _parseMode(modeStr);
    if (mode == VWIRE_GPIO_DISABLED) continue;
    if (addPin(pinName, mode, interval)) configured++;
  }
  return configured;
}

bool VwireGPIO::addPin(const char* pinName, VwireGPIOMode mode,
                        uint16_t readInterval) {
  uint8_t gpio = _resolvePinNumber(pinName);
  if (gpio == 255) return false;
  return addPin(pinName, gpio, mode, readInterval);
}

bool VwireGPIO::addPin(const char* pinName, uint8_t gpioNumber,
                        VwireGPIOMode mode, uint16_t readInterval) {
  int idx = _findPin(pinName);
  if (idx < 0) {
    idx = _findFreeSlot();
    if (idx < 0) return false;
    _count++;
  }

  VwireGPIOPin& p = _pins[idx];
  strncpy(p.pinName, pinName, sizeof(p.pinName) - 1);
  p.pinName[sizeof(p.pinName) - 1] = '\0';
  for (char* c = p.pinName; *c; c++) *c = toupper(*c);

  p.gpioNumber = gpioNumber;
  p.mode = mode;
  p.active = true;
  p.lastValue = -32768;
  p.lastRead = 0;
  p.flags = 0;

  if (readInterval == 0) {
    p.readInterval = VWIRE_GPIO_READ_INTERVAL;
  } else if (readInterval < VWIRE_GPIO_MIN_READ_INTERVAL) {
    p.readInterval = VWIRE_GPIO_MIN_READ_INTERVAL;
  } else if (readInterval > VWIRE_GPIO_MAX_READ_INTERVAL) {
    p.readInterval = VWIRE_GPIO_MAX_READ_INTERVAL;
  } else {
    p.readInterval = readInterval;
  }

  _applyHardwareMode(p);
  return true;
}

bool VwireGPIO::removePin(const char* pinName) {
  int idx = _findPin(pinName);
  if (idx < 0) return false;
  _pins[idx].active = false;
  _count--;
  return true;
}

void VwireGPIO::clearAll() {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    _pins[i].active = false;
  }
  _count = 0;
}

// =============================================================================
// PIN I/O
// =============================================================================

void VwireGPIO::write(const char* pinName, int value) {
  int idx = _findPin(pinName);
  if (idx < 0) return;
  _writeHardware(_pins[idx], value);
  _pins[idx].lastValue = value;
}

int VwireGPIO::read(const char* pinName) const {
  return getPinValue(pinName);
}

void VwireGPIO::send(const char* pinName, int value) {
  _publishValue(pinName, value);
}

bool VwireGPIO::handleCommand(const char* pinName, int value) {
  int idx = _findPin(pinName);
  if (idx < 0) return false;

  VwireGPIOPin& p = _pins[idx];
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

uint8_t VwireGPIO::getPinCount() const { return _count; }

bool VwireGPIO::hasPin(const char* pinName) const {
  return _findPin(pinName) >= 0;
}

int VwireGPIO::getPinValue(const char* pinName) const {
  int idx = _findPin(pinName);
  if (idx < 0) return -1;
  return _pins[idx].lastValue;
}

// =============================================================================
// PUBLISH HELPER
// =============================================================================

void VwireGPIO::_publishValue(const char* pinName, int value) {
  if (!_vwire) return;

  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/pin/%s",
           _vwire->getDeviceId(), pinName);

  char valStr[16];
  snprintf(valStr, sizeof(valStr), "%d", value);

  _vwire->publish(topic, valStr);
}

// =============================================================================
// PRIVATE HELPERS
// =============================================================================

int VwireGPIO::_findPin(const char* pinName) const {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    if (!_pins[i].active) continue;
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

int VwireGPIO::_findFreeSlot() const {
  for (uint8_t i = 0; i < VWIRE_MAX_GPIO_PINS; i++) {
    if (!_pins[i].active) return i;
  }
  return -1;
}

VwireGPIOMode VwireGPIO::_parseMode(const char* modeStr) {
  if (!modeStr) return VWIRE_GPIO_DISABLED;
  if (strcasecmp(modeStr, "OUTPUT") == 0)       return VWIRE_GPIO_OUTPUT;
  if (strcasecmp(modeStr, "INPUT") == 0)        return VWIRE_GPIO_INPUT;
  if (strcasecmp(modeStr, "INPUT_PULLUP") == 0) return VWIRE_GPIO_INPUT_PULLUP;
  if (strcasecmp(modeStr, "PWM") == 0)          return VWIRE_GPIO_PWM;
  if (strcasecmp(modeStr, "ANALOG_INPUT") == 0) return VWIRE_GPIO_ANALOG_INPUT;
  return VWIRE_GPIO_DISABLED;
}

uint8_t VwireGPIO::_resolvePinNumber(const char* pinName) {
  if (!pinName || pinName[0] == '\0') return 255;

  char prefix = toupper(pinName[0]);
  int num = atoi(pinName + 1);

  if (prefix == 'D') {
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
      return (uint8_t)num;
    #endif
  }
  else if (prefix == 'A') {
    #if defined(VWIRE_BOARD_ESP8266)
      return 17;  // ESP8266 A0 = GPIO 17 (TOUT)
    #elif defined(A0)
      return (uint8_t)(A0 + num);
    #else
      return (uint8_t)num;
    #endif
  }

  return 255;
}

void VwireGPIO::_applyHardwareMode(VwireGPIOPin& pin) {
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
      #if defined(VWIRE_BOARD_ESP32)
        // ESP32: analogRead works without explicit mode
      #else
        pinMode(pin.gpioNumber, INPUT);
      #endif
      break;
    default:
      break;
  }
}

int VwireGPIO::_readHardware(const VwireGPIOPin& pin) const {
  if (pin.mode == VWIRE_GPIO_ANALOG_INPUT) {
    return analogRead(pin.gpioNumber);
  }
  return digitalRead(pin.gpioNumber);
}

void VwireGPIO::_writeHardware(VwireGPIOPin& pin, int value) {
  // Smart write:
  //   0 or 1   → digitalWrite  (clean digital HIGH / LOW)
  //   2 – 255  → analogWrite   (PWM duty cycle, platform-specific)

  value = constrain(value, 0, 255);

  if (value <= 1) {
    #if defined(VWIRE_BOARD_ESP32)
      #if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
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

  // PWM (value 2 – 255)
  #if defined(VWIRE_BOARD_ESP32)
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      analogWrite(pin.gpioNumber, value);
    #else
      uint8_t ch = (uint8_t)(&pin - _pins);
      if (!(pin.flags & VWIRE_GPIO_FLAG_PWM_INIT)) {
        ledcSetup(ch, 5000, 8);
        ledcAttachPin(pin.gpioNumber, ch);
        pin.flags |= VWIRE_GPIO_FLAG_PWM_INIT;
      }
      ledcWrite(ch, value);
    #endif
  #elif defined(VWIRE_BOARD_ESP8266)
    analogWrite(pin.gpioNumber, (uint8_t)value);
  #else
    analogWrite(pin.gpioNumber, value);
  #endif
}

#else

bool VwireClass::enableGPIO() {
  _debugPrint("[Vwire] GPIO is disabled in this build");
  return false;
}

bool VwireClass::useGPIO(VwireGPIO& gpio) {
  (void)gpio;
  _debugPrint("[Vwire] GPIO is disabled in this build");
  return false;
}

bool VwireClass::isGPIOEnabled() const {
  return false;
}

bool VwireClass::addGPIOPin(const char* pinName, VwireGPIOMode mode,
                            uint16_t readInterval) {
  (void)pinName;
  (void)mode;
  (void)readInterval;
  return false;
}

bool VwireClass::addGPIOPin(const char* pinName, uint8_t gpioNumber,
                            VwireGPIOMode mode, uint16_t readInterval) {
  (void)pinName;
  (void)gpioNumber;
  (void)mode;
  (void)readInterval;
  return false;
}

bool VwireClass::removeGPIOPin(const char* pinName) {
  (void)pinName;
  return false;
}

void VwireClass::clearGPIOPins() {}

void VwireClass::gpioWrite(const char* pinName, int value) {
  (void)pinName;
  (void)value;
}

int VwireClass::gpioRead(const char* pinName) const {
  (void)pinName;
  return -1;
}

void VwireClass::gpioSend(const char* pinName, int value) {
  (void)pinName;
  (void)value;
}

uint8_t VwireClass::getGPIOPinCount() const {
  return 0;
}

VwireGPIO::VwireGPIO() : _vwire(nullptr), _count(0) {}

void VwireGPIO::begin(VwireClass& vwire) {
  (void)vwire;
}

bool VwireGPIO::addPin(const char* pinName, VwireGPIOMode mode,
                       uint16_t readInterval) {
  (void)pinName;
  (void)mode;
  (void)readInterval;
  return false;
}

bool VwireGPIO::addPin(const char* pinName, uint8_t gpioNumber,
                       VwireGPIOMode mode, uint16_t readInterval) {
  (void)pinName;
  (void)gpioNumber;
  (void)mode;
  (void)readInterval;
  return false;
}

bool VwireGPIO::removePin(const char* pinName) {
  (void)pinName;
  return false;
}

void VwireGPIO::clearAll() {}

void VwireGPIO::write(const char* pinName, int value) {
  (void)pinName;
  (void)value;
}

int VwireGPIO::read(const char* pinName) const {
  (void)pinName;
  return -1;
}

void VwireGPIO::send(const char* pinName, int value) {
  (void)pinName;
  (void)value;
}

uint8_t VwireGPIO::getPinCount() const {
  return 0;
}

bool VwireGPIO::hasPin(const char* pinName) const {
  (void)pinName;
  return false;
}

int VwireGPIO::getPinValue(const char* pinName) const {
  (void)pinName;
  return -1;
}

void VwireGPIO::onAttach(VwireClass& vwire) {
  (void)vwire;
}

void VwireGPIO::onConnect() {}

bool VwireGPIO::onMessage(const char* topic, const char* payload) {
  (void)topic;
  (void)payload;
  return false;
}

void VwireGPIO::onRun() {}

#endif
