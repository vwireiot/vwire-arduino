# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-03-31

### Major Release

This release supersedes the previous public release, **1.0.0**, and folds in the configuration, GPIO architecture, and logging changes that were previously tracked as internal 1.1.0 and 1.2.0 work. Because the public API is no longer backward compatible with 1.0.0-era usage, the version is bumped to **2.0.0**.

### Added
- **`Vwire.logTo(stream)`** and **`Vwire.onLog(callback)`** for the main client
- **`VwireProvision.logTo(stream)`**, **`VwireProvision.onLog(callback)`**, and **`VwireProvision.disableLog()`** for consistent provisioning logs
- **`Vwire.enableGPIO()`** and core GPIO convenience methods: `addGPIOPin`, `removeGPIOPin`, `clearGPIOPins`, `gpioWrite`, `gpioRead`, `gpioSend`, `getGPIOPinCount`
- **`VwireAddon` base class** and addon lifecycle hooks for modular extensions
- **`Vwire.publish(topic, payload)`** and **`Vwire.subscribe(topic)`** for addons and advanced use cases
- **15_Digital_Pins** and **16_Analog_Pins** examples for cloud-managed GPIO usage

### Changed
- **Configuration API simplified** to `Vwire.config(authToken, deviceId = nullptr, transport = VWIRE_TRANSPORT_TCP_SSL)`
- **TLS is the default transport**
- **GPIO implementation made modular** while keeping a simple core-facing API for sketches
- **Optional feature model clarified**: normal sketches save flash/RAM by simply not enabling optional modules, while advanced builds can hard-strip features with global `VWIRE_DISABLE_*` flags
- **Logging is disabled by default** and follows the same model in both core and provisioning
- **Examples and README updated** for the 2.0.0 API surface

### Removed
- **`gpioEnabled` parameter** from `config()`
- **Old built-in GPIO manager coupling** from the core implementation
- **Deprecated setup patterns based on the old multi-step configuration flow** from the recommended documentation

### Breaking Changes
- `Vwire.config(AUTH_TOKEN, DEVICE_ID, transport, gpioEnabled)` → `Vwire.config(AUTH_TOKEN, DEVICE_ID, transport)`
- Sketches that relied on the old always-on GPIO behavior must now call `Vwire.enableGPIO()` or attach `VwireGPIO` explicitly
- Old debug setup using only `setDebug(true)` / `setDebugStream()` is no longer the recommended API; use `logTo()` or `onLog()`

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
- **13_AP_Mode_Provisioning**: Example for access point configuration with stored-credentials fallback
- **14_OEM_PreProvisioned**: Example for manufacturer pre-provisioned devices

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
