# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03

### Simplified Configuration API 🎯

The library now uses a **single `config()` method** with sensible defaults, replacing the previous multi-step setup. GPIO pin management is enabled by default.

**Before (1.0.0):**
```cpp
Vwire.config(AUTH_TOKEN);
Vwire.setDeviceId(DEVICE_ID);
Vwire.setTransport(VWIRE_TRANSPORT_TCP_SSL);
Vwire.enableGPIO();
```

**After (1.1.0):**
```cpp
Vwire.config(AUTH_TOKEN, DEVICE_ID);
```

### Changed
- **`config()` consolidated**: Single method signature with defaults:
  `config(authToken, deviceId = nullptr, transport = TLS, gpioEnabled = true)`
- **GPIO enabled by default**: Cloud-managed pin control is now active out of the box — no need to call `enableGPIO()`
- **TLS is the default transport**: No need to call `setTransport(VWIRE_TRANSPORT_TCP_SSL)` — it's the default
- **Device ID as parameter**: Pass device ID directly in `config()` instead of calling `setDeviceId()` separately
- **01_Basic example rewritten**: Minimal ~90-line example focused on LED control + GPIO
- **All 18 examples updated**: Use the new single-call `config()` pattern
- **Updated README.md**: Full API reference rewritten for new `config()` signature, added GPIO Pin Management section

### Removed
- **`config(authToken, server, port)` overload**: Custom server configuration removed
- **`config(VwireSettings&)` overload**: Settings struct configuration removed
- **TRANSPORT constant from examples**: No longer needed since TLS is the default

### Backward Compatibility
- `Vwire.config(AUTH_TOKEN)` still works (device ID defaults to auth token)
- `setDeviceId()`, `setTransport()`, and `enableGPIO()` still available as public API
- Existing sketches using the old multi-step pattern will compile without changes

### Added
- **17_Digital_Pins example**: Cloud-controlled digital pin management with Smart Write (auto PWM)
- **18_Analog_Pins example**: Cloud-monitored analog inputs with configurable read intervals
- **GPIO API documentation**: Full reference for `addGPIOPin()`, `gpioWrite()`, `gpioRead()`, `gpioSend()`, `getGPIO()`

---

## [1.0.0] - 2026-03

### Added - WiFi Provisioning Support 📱

New `VwireProvisioning` module for configuring WiFi credentials via a browser — no hardcoding credentials in firmware.

### Added
- **VwireProvisioning.h**: WiFi provisioning header with AP Mode support
- **VwireProvisioning.cpp**: Full implementation for ESP32 and ESP8266
- **AP Mode support**: Device creates hotspot with embedded web configuration page
- **Credential storage**: Persistent storage using ESP32 Preferences (NVS) or ESP8266 EEPROM
- **Provisioning callbacks**: `onStateChange()`, `onCredentialsReceived()`, `onProgress()`
- **14_AP_Mode_Provisioning**: Example for access point configuration
- **15_Auto_Provisioning**: Production-ready example (stored credentials or AP Mode fallback)
- **16_OEM_PreProvisioned**: Example for manufacturer pre-provisioned devices

### Removed
- **SmartConfig / ESP-Touch support**: Removed `startSmartConfig()` — use AP Mode instead

### Changed
- Updated library.properties with VwireProvisioning.h include
- Updated keywords.txt with provisioning keywords
- Enhanced README.md with comprehensive provisioning documentation

### Provisioning API
```cpp
#include <VwireProvisioning.h>

// Check stored credentials
VwireProvision.hasCredentials();
VwireProvision.getSSID();
VwireProvision.getAuthToken();

// AP Mode
VwireProvision.startAPMode(password, timeout);

// Callbacks
VwireProvision.onStateChange(callback);
VwireProvision.onCredentialsReceived(callback);
```

---

## [0.0.1] - 2023

### Added
- Initial release
- Basic MQTT connectivity
- Virtual pin support
- Simple connection/disconnection handlers
