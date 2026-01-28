# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.0.0] - 2026-01

### 🚀 Major Release - Rebranded to Vwire IOT

This is a major release with significant changes including a complete rebrand from IoTech to Vwire IOT.

### ⚠️ Breaking Changes
- **Library renamed**: IoTech → VwireIOT
- **Header file renamed**: `IoTech.h` → `Vwire.h`
- **Global instance renamed**: `IoTech` → `Vwire`
- **WebSocket support removed**: Use MQTTS (TLS) instead for secure connections
- **Default server changed**: `mqtt.vwireiot.com` → `mqtt.vwire.io`
- **Default transport changed**: Now defaults to TLS on port 8883

### Added
- **Vwire.h**: New main header file
- **Vwire.cpp**: New main implementation file
- **VwireConfig.h**: New configuration file with board detection
- **New macros**: `VWIRE_RECEIVE()`, `VWIRE_READ()`, `VWIRE_CONNECTED()`, `VWIRE_DISCONNECTED()`
- **New constants**: `VWIRE_TRANSPORT_TCP`, `VWIRE_TRANSPORT_TCP_SSL`
- **New state enum**: `VwireState` with states like `VWIRE_STATE_CONNECTED`
- **New error enum**: `VwireError` with codes like `VWIRE_ERR_MQTT_FAILED`

### Changed
- **Recommended transport**: MQTTS (TLS) on port 8883 is now the default and recommended option
- **Simplified transports**: Only TCP (1883) and TCP+TLS (8883) supported
- **Updated examples**: All examples updated to use Vwire naming
- **Improved documentation**: Comprehensive README with full API reference
- **Better TLS handling**: Improved secure client setup for ESP32/ESP8266

### Removed
- **WebSocket transport**: Removed `IOTECH_TRANSPORT_WEBSOCKET` and `IOTECH_TRANSPORT_WEBSOCKET_SSL`
- **WebSocket files**: Removed `IoTechWebSocket.h` and `IoTechWebSocket.cpp`
- **WebSocket example**: Removed `06_WebSocket_Secure` example
- **WebSockets dependency**: No longer requires WebSockets library
- **wsPath setting**: Removed from settings struct (no longer needed)
- **Legacy IoTech aliases**: No backward compatibility - use Vwire naming only

---

## [2.0.1] - 2026-01-17

### Added
- **Real WebSocket Support**: Implemented proper WebSocket transport using WebSocketsClient library
- **IoTechWebSocket.h/cpp**: New WebSocket stream wrapper that provides Stream interface for PubSubClient
- **WebSockets Dependency**: Added links2004/WebSockets library for proper WSS support

### Changed
- **MQTT Broker Configuration**: All examples now have clear MQTT_BROKER, MQTT_PORT, and MQTT_TRANSPORT variables at the top
- **Consistent Example Structure**: Separated WiFi, Authentication, and MQTT Broker configuration sections

### Fixed
- **WSS Connection Issue**: Fixed ESP8266/ESP32 not connecting over WebSocket Secure (port 443)
- Previously, selecting WSS transport still used raw TCP which doesn't work with nginx/reverse proxy WebSocket endpoints

---

## [2.0.0] - 2026-01

### Added
- **WebSocket Support**: MQTT over WebSocket and WebSocket+SSL for firewall-friendly connections
- **Multi-Board Support**: Automatic detection and configuration for ESP32, ESP8266, RP2040, SAMD, AVR
- **Transport Selection**: Choose between TCP, TCP+SSL, WebSocket, WebSocket+SSL
- **OTA Updates**: Over-the-air firmware updates for ESP32 and ESP8266
- **Enhanced VirtualPin**: Array support, formatted output, type conversion methods
- **Auto Reconnect**: Automatic WiFi and MQTT reconnection with configurable settings
- **Heartbeat**: Keep-alive with server status monitoring
- **Debug Mode**: Comprehensive debug output for troubleshooting
- **Device Info**: Methods to get device ID, board name, free heap, uptime, WiFi RSSI
- **New Examples**:
  - 01_Basic: Generic starter example
  - 02_ESP32_Complete: Full ESP32 features with touch, PWM, preferences
  - 03_ESP8266_Complete: ESP8266 with LittleFS storage
  - 04_DHT_SensorStation: Temperature/humidity monitoring with alerts
  - 05_Relay_Control: Multi-relay control with physical buttons
  - 06_WebSocket_Secure: WSS connection through firewalls
  - 07_RGB_LED_Strip: NeoPixel/WS2812B with effects
  - 08_Motor_Servo: DC motor and servo control
  - 09_SelfHosted_Server: Custom server connection
  - 10_Minimal: Simplest possible template

### Changed
- **Refactored Architecture**: Split into IoTechConfig.h, IoTech.h, IoTech.cpp
- **Improved API**: More consistent method naming and parameters
- **Better Error Handling**: Enum-based error codes with descriptive messages

---

## [1.0.0] - 2023

### Added
- Initial release
- Basic MQTT connectivity
- Virtual pin support
- Simple connection/disconnection handlers
