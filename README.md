# Vwire IOT Arduino Library

Professional IoT platform library for Arduino, ESP32, ESP8266 and compatible boards.  
Connect your microcontrollers to **Vwire IOT** cloud platform via secure MQTT.

[![Version](https://img.shields.io/badge/version-3.1.0-blue.svg)](https://github.com/vwireiot/vwire-arduino)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

🌐 **Cloud Portal**: [https://vwire.io](https://vwire.io)

---

## ⚡ Quick API Reference

| Function | Direction | Description |
|----------|:---------:|-------------|
| `Vwire.virtualSend(pin, value)` | Device → Cloud | Send data to dashboard |
| `VWIRE_RECEIVE(pin) { }` | Cloud → Device | Handle incoming data |
| `VWIRE_CONNECTED() { }` | - | Called when connected |
| `VWIRE_DISCONNECTED() { }` | - | Called when disconnected |
| `Vwire.config(AUTH_TOKEN)` | - | Configure with default server |
| `Vwire.begin(SSID, PASS)` | - | Connect to WiFi & cloud |
| `Vwire.run()` | - | Process messages (call in loop) |

---

## 🌟 Features

- 🔐 **Secure Connections**: MQTT over TLS/SSL encryption (port 8883) - **Recommended**
- 📡 **Standard MQTT**: Plain TCP support (port 1883) for boards without SSL
- 📊 **Virtual Pins**: Bidirectional data exchange with dashboard (256 pins)
- ⏱️ **VwireTimer**: Non-blocking timers for all Arduino boards
- 🔔 **Notifications**: Push notifications, email alerts, and persistent alarms
- 🔄 **OTA Updates**: Over-the-air firmware updates (ESP32/ESP8266)
- ⚡ **Auto Reconnect**: Automatic connection recovery with configurable intervals
- 🎯 **Multi-Platform**: ESP32, ESP8266, RP2040, SAMD, and more
- ✅ **Reliable Delivery**: Optional application-level ACK for guaranteed message delivery
- 📱 **WiFi Provisioning**: SmartConfig and AP Mode for mobile app configuration

---

## 🔧 Supported Boards

| Board | MQTT (TCP) | MQTTS (TLS) | OTA Updates |
|-------|:----------:|:-----------:|:-----------:|
| **ESP32** | ✅ | ✅ | ✅ |
| **ESP8266** | ✅ | ✅ | ✅ |
| **RP2040 (Pico W)** | ✅ | ⚠️ Limited | ❌ |
| **Arduino WiFi** | ✅ | ❌ | ❌ |
| **SAMD** | ✅ | ❌ | ❌ |

> **Note**: For secure IoT deployments, use **ESP32** or **ESP8266** with **MQTTS (TLS)** enabled.

---

## 📦 Installation

### Arduino IDE

1. Download this library as ZIP from [GitHub](https://github.com/vwireiot/vwire-arduino)
2. Go to **Sketch → Include Library → Add .ZIP Library**
3. Select the downloaded ZIP file
4. Install dependencies: `PubSubClient` and `ArduinoJson` from Library Manager

---

## 🚀 Quick Start

```cpp
#include <Vwire.h>

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

// Vwire IOT Authentication
const char* AUTH_TOKEN = "YOUR_AUTH_TOKEN";
const char* DEVICE_ID  = "YOUR_DEVICE_ID"; // VW-XXXXXX (OEM) or VU-XXXXXX (user-created)

// Transport Configuration (optional - TLS is default)
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, for boards without SSL
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// ===== HANDLERS (auto-registered!) =====

// Called when cloud sends data to V0 (e.g., button press)
VWIRE_RECEIVE(V0) {
  int value = param.asInt();  // 'param' is the VirtualPin
  digitalWrite(LED_BUILTIN, value);
  Serial.printf("V0 received: %d\n", value);
}

// Called when connected to server
VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  Vwire.syncVirtual(V0);  // Request stored value from server
}

// Called when disconnected
VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.config(AUTH_TOKEN);
  // Recommended (Option A): use Device ID for topic routing (token remains only credential)
  Vwire.setDeviceId(DEVICE_ID);
  Vwire.setTransport(TRANSPORT);
  Vwire.setDebug(true);
  
  // Connect - handlers are auto-registered by macros!
  if (Vwire.begin(WIFI_SSID, WIFI_PASS)) {
    Serial.println("Connected!");
  }
}

// ===== LOOP =====
void loop() {
  Vwire.run();  // Must call frequently!
  
  // Send data every 5 seconds
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend > 5000) {
    lastSend = millis();
    Vwire.virtualSend(V1, millis() / 1000);  // Send uptime to V1
  }
}
```

---

## 📡 Transport Configuration

### Available Transports

| Transport | Constant | Port | Encryption | Recommendation |
|-----------|----------|:----:|:----------:|----------------|
| **MQTTS (TLS)** | `VWIRE_TRANSPORT_TCP_SSL` | 8883 | ✅ Encrypted | ✅ **RECOMMENDED** |
| MQTT (TCP) | `VWIRE_TRANSPORT_TCP` | 1883 | ❌ Plain | For boards without SSL support |

### Vwire IOT Cloud (Default)

```cpp
// Simplest configuration - uses mqtt.vwire.io with TLS
Vwire.config(AUTH_TOKEN);
Vwire.setTransport(VWIRE_TRANSPORT_TCP_SSL);
```

### For Boards Without SSL Support

```cpp
// Use plain TCP (port 1883) for boards that don't support TLS
Vwire.config(AUTH_TOKEN);
Vwire.setTransport(VWIRE_TRANSPORT_TCP);
```

> 🌐 Sign up for free at [https://vwire.io](https://vwire.io) to get your AUTH_TOKEN

---

## 📚 Complete API Reference

### Configuration Functions

#### `Vwire.config(authToken)`
Configure with default server (mqtt.vwire.io). **Recommended for most users.**

```cpp
Vwire.config("your-auth-token-here");
```

#### `Vwire.config(authToken, server, port)`
Configure with custom MQTT server (for advanced/self-hosted setups).

```cpp
Vwire.config("your-token", "custom-server.com", 8883);
```

#### `Vwire.config(settings)`
Configure with full settings structure.

```cpp
VwireSettings settings;
strncpy(settings.authToken, "your-token", VWIRE_MAX_TOKEN_LENGTH);
strncpy(settings.server, "mqtt.vwire.io", VWIRE_MAX_SERVER_LENGTH);
settings.port = 8883;
settings.transport = VWIRE_TRANSPORT_TCP_SSL;
settings.autoReconnect = true;
Vwire.config(settings);
```

#### `Vwire.setTransport(transport)`
Set transport protocol.

| Value | Description |
|-------|-------------|
| `VWIRE_TRANSPORT_TCP_SSL` | MQTT over TLS (port 8883) - **Recommended** |
| `VWIRE_TRANSPORT_TCP` | Plain MQTT (port 1883) |

```cpp
Vwire.setTransport(VWIRE_TRANSPORT_TCP_SSL);
```

#### `Vwire.setAutoReconnect(enable)`
Enable/disable automatic reconnection.

```cpp
Vwire.setAutoReconnect(true);   // Default: enabled
Vwire.setAutoReconnect(false);  // Disable auto-reconnect
```

#### `Vwire.setReconnectInterval(milliseconds)`
Set reconnection attempt interval (default: 5000ms).

```cpp
Vwire.setReconnectInterval(10000);  // Try every 10 seconds
```

#### `Vwire.setHeartbeatInterval(milliseconds)`
Set heartbeat interval (default: 30000ms).

```cpp
Vwire.setHeartbeatInterval(60000);  // Heartbeat every 60 seconds
```

---

### Reliable Delivery (Application-Level ACK)

Enable guaranteed message delivery for critical data. When enabled, the server acknowledges each message, and the device automatically retries if no ACK is received.

#### When to Use Reliable Delivery

| Use Case | Reliable Delivery | Reason |
|----------|:-----------------:|--------|
| **Energy Metering** | ✅ Yes | Every reading matters for billing |
| **Event Logging** | ✅ Yes | Button presses, alerts must not be lost |
| **Infrequent Sensors** | ✅ Yes | Daily readings are valuable |
| **High-Frequency Telemetry** | ❌ No | Missing one of 1000 readings is OK |
| **Real-time Streaming** | ❌ No | Fresh data matters more than completeness |

#### `Vwire.setReliableDelivery(enable)`
Enable or disable reliable delivery (default: disabled for backward compatibility).

```cpp
Vwire.setReliableDelivery(true);   // Enable ACK-based delivery
Vwire.setReliableDelivery(false);  // Standard fire-and-forget (default)
```

#### `Vwire.setAckTimeout(milliseconds)`
Set how long to wait for server ACK before retry (default: 5000ms).

```cpp
Vwire.setAckTimeout(3000);  // 3 second timeout
```

#### `Vwire.setMaxRetries(count)`
Set maximum retry attempts before dropping message (default: 3).

```cpp
Vwire.setMaxRetries(5);  // Try up to 5 times
```

#### `Vwire.onDeliveryStatus(callback)`
Register callback for delivery success/failure notifications.

```cpp
void onDelivery(const char* msgId, bool success) {
  if (success) {
    Serial.printf("✓ Message %s delivered\n", msgId);
  } else {
    Serial.printf("✗ Message %s failed\n", msgId);
    // Optionally: store to flash for later retry
  }
}

void setup() {
  Vwire.setReliableDelivery(true);
  Vwire.onDeliveryStatus(onDelivery);
  // ...
}
```

#### `Vwire.getPendingCount()`
Get the number of messages waiting for acknowledgment.

```cpp
uint8_t pending = Vwire.getPendingCount();
Serial.printf("Pending messages: %d\n", pending);
```

#### `Vwire.isDeliveryPending()`
Check if any messages are awaiting acknowledgment.

```cpp
if (Vwire.isDeliveryPending()) {
  Serial.println("Waiting for server confirmation...");
}
```

#### Complete Reliable Delivery Example

```cpp
#include <Vwire.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";
const char* AUTH_TOKEN = "YOUR_AUTH_TOKEN";

// Track delivery statistics
unsigned long successCount = 0;
unsigned long failCount = 0;

void onDeliveryResult(const char* msgId, bool success) {
  if (success) {
    successCount++;
    Serial.printf("✓ [%s] Delivered (total: %lu)\n", msgId, successCount);
  } else {
    failCount++;
    Serial.printf("✗ [%s] Failed (total: %lu)\n", msgId, failCount);
  }
}

void setup() {
  Serial.begin(115200);
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.config(AUTH_TOKEN);
  Vwire.setTransport(VWIRE_TRANSPORT_TCP_SSL);
  Vwire.setDebug(true);
  
  // Enable reliable delivery
  Vwire.setReliableDelivery(true);
  Vwire.setAckTimeout(5000);    // 5 second timeout
  Vwire.setMaxRetries(3);       // 3 retry attempts
  Vwire.onDeliveryStatus(onDeliveryResult);
  
  // Connect
  Vwire.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  Vwire.run();
  
  // Send energy meter reading every minute (must not be lost!)
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend > 60000) {
    lastSend = millis();
    
    float kWh = readEnergyMeter();
    Vwire.virtualSend(V0, kWh);  // Automatically uses reliable delivery
    
    Serial.printf("Sent: %.2f kWh (pending: %d)\n", kWh, Vwire.getPendingCount());
  }
}
```

#### Memory Considerations

Reliable delivery uses additional memory for the pending message queue:

| Setting | Value | Memory Impact |
|---------|-------|---------------|
| Max pending messages | 10 | ~800 bytes |
| Message ID length | 15 chars | Included above |
| Value buffer | 64 chars | Per message |

For memory-constrained devices (ESP8266), monitor free heap:

```cpp
Serial.printf("Free heap: %u bytes\n", Vwire.getFreeHeap());
```

---

### WiFi Provisioning (SmartConfig & AP Mode)

New in v3.1.0! Configure WiFi credentials and device token via the Vwire mobile app without hardcoding them in your sketch.

#### Overview

| Method | Description | Best For |
|--------|-------------|----------|
| **SmartConfig** | App broadcasts credentials to device | Consumer products, easy setup |
| **AP Mode** | Device creates hotspot with config page | Networks without SmartConfig support |
| **Auto** | SmartConfig first, then fallback to AP | **Recommended for production** |

#### Basic Usage

```cpp
#include <Vwire.h>
#include <VwireProvisioning.h>

void setup() {
  Serial.begin(115200);
  
  // Check if we have stored credentials from previous provisioning
  if (VwireProvision.hasCredentials()) {
    // Use stored credentials
    Vwire.config(VwireProvision.getAuthToken());
    Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword());
  } else {
    // No credentials - start SmartConfig provisioning
    VwireProvision.startSmartConfig();
  }
}

void loop() {
  // Run provisioning if active
  if (VwireProvision.isProvisioning()) {
    VwireProvision.run();
    return;
  }
  
  // Normal operation after provisioning
  if (VwireProvision.getState() == VWIRE_PROV_SUCCESS && !Vwire.connected()) {
    Vwire.config(VwireProvision.getAuthToken());
    Vwire.begin();  // WiFi already connected
  }
  
  Vwire.run();
}
```

#### SmartConfig Provisioning

Uses ESP-Touch protocol to receive credentials broadcast by the mobile app.

```cpp
#include <VwireProvisioning.h>

// Start SmartConfig with 2-minute timeout
VwireProvision.startSmartConfig(120000);

// Or with callback
VwireProvision.onStateChange([](VwireProvisioningState state) {
  if (state == VWIRE_PROV_SUCCESS) {
    Serial.println("Provisioned!");
  }
});
VwireProvision.startSmartConfig();
```

#### AP Mode Provisioning

Device creates a WiFi hotspot with a configuration web page.

```cpp
#include <VwireProvisioning.h>

// Start AP Mode with password (5-minute timeout)
VwireProvision.startAPMode("vwire123", 300000);

// Or open network
VwireProvision.startAPMode(nullptr, 300000);

// Get AP info for display
Serial.printf("Connect to: %s\n", VwireProvision.getAPSSID());
Serial.printf("Open: http://%s\n", VwireProvision.getAPIP().c_str());
```

#### OEM Mode (Pre-Provisioned Devices)

**New in v3.1.0!** OEM mode is designed for manufacturers who pre-provision devices before shipping to customers.

**Two Provisioning Workflows:**

| Mode | Token Source | Portal Shows | Use Case |
|------|--------------|--------------|----------|
| **End-User Mode** (default) | User enters from dashboard | WiFi + Token fields | Consumer sells device, buyer provisions |
| **OEM Mode** | Pre-configured in firmware | WiFi only | Manufacturer pre-provisions, customer adds WiFi |

**OEM Workflow:**

1. Manufacturer creates devices in bulk via Admin Panel
2. Each device gets unique Device ID and Auth Token
3. Manufacturer flashes firmware with pre-configured token
4. Customer receives device and only provides WiFi credentials
5. When customer claims device, template project is auto-created

**End-User Workflow (Default):**

1. User flashes generic firmware (no token)
2. User creates device in their dashboard
3. User configures device via portal with WiFi + Token
4. Device connects to their project

**Example - OEM Mode:**

```cpp
#include <Vwire.h>
#include <VwireProvisioning.h>

// Pre-configured from Admin Panel bulk provisioning
#define VWIRE_AUTH_TOKEN "your-token-from-admin-csv"

void setup() {
  // Configure with hardcoded token (OEM token is in firmware, not EEPROM)
  Vwire.config(VWIRE_AUTH_TOKEN);
  
  if (VwireProvision.hasCredentials()) {
    // Use stored WiFi credentials + hardcoded token
    Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword());
  } else {
    // Start AP Mode in OEM mode (WiFi only, no token field)
    // Token stays in firmware, only WiFi credentials stored in EEPROM
    VwireProvision.startAPMode("vwire123", 0, true);  // true = OEM mode
  }
}

void loop() {
  Vwire.run();
  VwireProvision.run();
}
```

**Example - End-User Mode (Default):**

```cpp
#include <Vwire.h>
#include <VwireProvisioning.h>

void setup() {
  if (VwireProvision.hasCredentials()) {
    // Use stored credentials (WiFi + Token)
    Vwire.config(VwireProvision.getAuthToken());
    Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword());
  } else {
    // Start AP Mode - user provides both WiFi and Token
    VwireProvision.startAPMode("vwire123", 0);  // false = End-user mode (default)
  }
}

void loop() {
  Vwire.run();
  VwireProvision.run();
}
```

**See also:** [Example 16_OEM_PreProvisioned](examples/16_OEM_PreProvisioned)

#### Provisioning States

| State | Description |
|-------|-------------|
| `VWIRE_PROV_IDLE` | Not provisioning |
| `VWIRE_PROV_SMARTCONFIG_WAIT` | Waiting for SmartConfig data |
| `VWIRE_PROV_AP_ACTIVE` | AP mode active, web server running |
| `VWIRE_PROV_CONNECTING` | Connecting to WiFi |
| `VWIRE_PROV_SUCCESS` | Provisioning complete |
| `VWIRE_PROV_FAILED` | Connection failed |
| `VWIRE_PROV_TIMEOUT` | Timeout occurred |

#### Credential Management

```cpp
// Check if credentials exist
if (VwireProvision.hasCredentials()) {
  Serial.printf("SSID: %s\n", VwireProvision.getSSID());
  Serial.printf("Token: %s\n", VwireProvision.getAuthToken());
}

// Clear stored credentials (factory reset)
VwireProvision.clearCredentials();

// Manually save credentials
VwireProvision.saveCredentials("MyWiFi", "password123", "device-token");
```

#### Callbacks

```cpp
// State change callback
VwireProvision.onStateChange([](VwireProvisioningState state) {
  switch (state) {
    case VWIRE_PROV_SMARTCONFIG_WAIT:
      Serial.println("Open Vwire app → Smart Config");
      break;
    case VWIRE_PROV_SUCCESS:
      Serial.println("Provisioning complete!");
      break;
    case VWIRE_PROV_TIMEOUT:
      // Switch to AP mode as fallback
      VwireProvision.startAPMode("vwire123");
      break;
  }
});

// Credentials received callback
VwireProvision.onCredentialsReceived([](const char* ssid, const char* pass, const char* token) {
  Serial.printf("Received: SSID=%s, Token=%s\n", ssid, token);
});

// Progress callback (SmartConfig)
VwireProvision.onProgress([](int percent, const char* msg) {
  Serial.printf("%d%% - %s\n", percent, msg);
});
```

#### Complete Auto-Provisioning Example

See [15_Auto_Provisioning](examples/15_Auto_Provisioning) for a production-ready implementation.

> **Note**: SmartConfig only works with 2.4GHz WiFi networks. For 5GHz networks, use AP Mode.

---

### Connection Functions

#### `Vwire.begin(ssid, password)`
Connect to WiFi and MQTT broker. Returns `true` on success.

```cpp
if (Vwire.begin("MyWiFi", "MyPassword")) {
  Serial.println("Connected!");
} else {
  Serial.println("Connection failed!");
}
```

#### `Vwire.begin()`
Connect to MQTT when WiFi is already configured.

```cpp
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) delay(500);
Vwire.begin();  // WiFi already connected
```

#### `Vwire.run()`
**Must be called frequently in `loop()`!** Handles MQTT messages, reconnection, and heartbeats.

```cpp
void loop() {
  Vwire.run();  // Always call this first!
  // Your code here...
}
```

#### `Vwire.connected()`
Check if connected to MQTT broker. Returns `true` if connected.

```cpp
if (Vwire.connected()) {
  Vwire.virtualSend(0, sensorValue);
}
```

#### `Vwire.disconnect()`
Disconnect from MQTT broker gracefully.

```cpp
Vwire.disconnect();
```

---

### Virtual Pin Operations

Virtual pins (V0-V255) enable bidirectional communication between your device and the cloud dashboard.

**Data Flow:**
- `virtualSend()` - Device → Cloud (send sensor data to dashboard)
- `VWIRE_RECEIVE()` - Cloud → Device (receive commands from dashboard)

#### `Vwire.virtualSend(pin, value)`
Send a value to a virtual pin (device → cloud). Supports all data types.

```cpp
// Integer
Vwire.virtualSend(0, 42);

// Float
Vwire.virtualSend(1, 23.5);

// Boolean
Vwire.virtualSend(2, true);

// String
Vwire.virtualSend(3, "Hello Vwire!");

// Character array
char msg[] = "Status OK";
Vwire.virtualSend(4, msg);
```

#### `Vwire.virtualSendf(pin, format, ...)`
Write formatted string to virtual pin (printf-style).

```cpp
float temp = 25.5;
float humidity = 60.2;
Vwire.virtualSendf(0, "%.1f°C, %.0f%%", temp, humidity);
```

#### `Vwire.virtualSendArray(pin, values, count)`
Write an array of values (comma-separated).

```cpp
// Integer array
int values[] = {10, 20, 30, 40, 50};
Vwire.virtualSendArray(0, values, 5);  // Sends "10,20,30,40,50"

// Float array (RGB example)
float rgb[] = {255.0, 128.0, 64.0};
Vwire.virtualSendArray(1, rgb, 3);  // Sends "255.00,128.00,64.00"
```

#### `Vwire.syncVirtual(pin)`
Request current value of a virtual pin from server.

```cpp
Vwire.syncVirtual(0);  // Request V0 value
```

#### `Vwire.syncAll()`
Request all virtual pin values from server.

```cpp
Vwire.syncAll();
```

---

### Event Handlers 

The library supports **Auto-registration** - handlers are automatically registered at startup using macros. No need to manually call `onVirtualReceive()` in setup()!

#### `VWIRE_RECEIVE(Vpin)` - Handle Incoming Data from Cloud

Define a handler that's **automatically called** when the cloud/dashboard sends data to your device.

```cpp
// Automatically registered - no setup() code needed!
VWIRE_RECEIVE(V0) {
  int value = param.asInt();    // 'param' is the VirtualPin& parameter
  Serial.printf("V0 received from cloud: %d\n", value);
  digitalWrite(LED_PIN, value);
}

VWIRE_RECEIVE(V1) {
  int brightness = param.asInt();
  analogWrite(LED_PIN, brightness);
}
```

#### `VWIRE_CONNECTED()` - Handle Connection Events

Define a handler that's **automatically called** when connected to the server.

```cpp
VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire!");
  
  // Request stored values from server (useful after power cycle)
  Vwire.sync(V0, V1, V2);  // Sync multiple pins
  // Or: Vwire.syncAll();  // Sync all pins
}
```

#### `VWIRE_DISCONNECTED()` - Handle Disconnection Events

Define a handler that's **automatically called** when disconnected.

```cpp
VWIRE_DISCONNECTED() {
  Serial.println("Connection lost!");
  digitalWrite(STATUS_LED, LOW);
}
```

#### Complete Example (Recommended Style)

```cpp
#include <Vwire.h>

// Configuration
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";
const char* AUTH_TOKEN = "YOUR_AUTH_TOKEN";

#define LED_PIN 2
#define SENSOR_PIN 34

// Handlers are automatically registered - just define them!
VWIRE_RECEIVE(V0) {
  bool ledState = param.asBool();
  digitalWrite(LED_PIN, ledState);
}

VWIRE_RECEIVE(V1) {
  int brightness = param.asInt();
  analogWrite(LED_PIN, brightness);
}

VWIRE_CONNECTED() {
  Serial.println("Connected!");
  Vwire.sync(V0, V1);  // Request stored values from server
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Simple configuration - uses default cloud server
  Vwire.config(AUTH_TOKEN);
  Vwire.setTransport(VWIRE_TRANSPORT_TCP_SSL);
  
  // No need to register handlers - macros do it automatically!
  Vwire.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  Vwire.run();
  
  // Send sensor data to cloud
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend > 2000) {
    lastSend = millis();
    Vwire.virtualSend(V2, analogRead(SENSOR_PIN));
  }
}
```

#### Virtual Pin Constants

Use `V0` through `V31` for pin numbers:

```cpp
Vwire.virtualSend(V0, 42);           // Same as virtualSend(0, 42)
Vwire.virtualSend(V1, temperature);
Vwire.sync(V0, V1, V2);               // Sync multiple pins
```

---

### Manual Handler Registration (Alternative)

You can also use manual registration if you prefer function-based handlers over macros:

#### `Vwire.onVirtualReceive(pin, handler)`
Manually register handler for incoming data from cloud.

```cpp
void onButtonPress(VirtualPin& pin) {
  int value = pin.asInt();
  digitalWrite(LED_PIN, value);
}

void setup() {
  Vwire.onVirtualReceive(0, onButtonPress);  // Manual registration
}
```

#### `Vwire.onConnect(handler)` / `Vwire.onDisconnect(handler)`
Manually register connection/disconnection handlers.

```cpp
void onConnected() {
  Serial.println("Connected!");
}

void setup() {
  Vwire.onConnect(onConnected);  // Manual registration
}
```

#### Using Lambda Functions

```cpp
Vwire.onVirtualReceive(0, [](VirtualPin& pin) {
  Serial.println(pin.asBool() ? "ON" : "OFF");
});

Vwire.onVirtualReceive(1, [](VirtualPin& pin) {
  analogWrite(LED_PIN, pin.asInt());
});

Vwire.onConnect([]() {
  Serial.println("Connected!");
});
```

#### `Vwire.onMessage(handler)`
Register handler for all raw MQTT messages.

```cpp
void onRawMessage(const char* topic, const char* payload) {
  Serial.printf("Topic: %s, Payload: %s\n", topic, payload);
}

Vwire.onMessage(onRawMessage);
```

---

### VirtualPin Class

The `VirtualPin` class provides type-safe access to received values.

#### Type Conversion Methods

```cpp
void handlePin(VirtualPin& pin) {
  // Get as different types
  int intVal = pin.asInt();
  float floatVal = pin.asFloat();
  double doubleVal = pin.asDouble();
  bool boolVal = pin.asBool();
  String strVal = pin.asString();
  const char* cstrVal = pin.asCString();
}
```

#### Array Access (Comma-Separated Values)

```cpp
void handleRGB(VirtualPin& pin) {
  // Value received: "255,128,64"
  int size = pin.getArraySize();  // Returns 3
  
  int r = pin.getArrayInt(0);     // 255
  int g = pin.getArrayInt(1);     // 128
  int b = pin.getArrayInt(2);     // 64
  
  // Or with floats
  float rf = pin.getArrayFloat(0);  // 255.0
  
  // Get as string
  String first = pin.getArrayElement(0);  // "255"
}
```

---

### VwireTimer Class

The `VwireTimer` class provides non-blocking timer functionality. It uses `millis()` internally and works on **all Arduino boards** (not just ESP).

#### Basic Usage

```cpp
#include <Vwire.h>

VwireTimer timer;

void sendSensorData() {
  Vwire.virtualSend(V0, analogRead(A0));
}

void blinkLED() {
  static bool state = false;
  state = !state;
  digitalWrite(LED_BUILTIN, state);
}

void setup() {
  // Call sendSensorData every 2 seconds
  timer.setInterval(2000, sendSensorData);
  
  // Blink LED every 500ms
  timer.setInterval(500, blinkLED);
  
  Vwire.config(AUTH_TOKEN);
  Vwire.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  Vwire.run();
  timer.run();  // Must call in loop!
}
```

#### Timer Methods

| Method | Description |
|--------|-------------|
| `setInterval(ms, callback)` | Repeat callback every `ms` milliseconds |
| `setTimeout(ms, callback)` | Call callback once after `ms` milliseconds |
| `setTimer(ms, callback, count)` | Call callback `count` times |
| `enable(id)` | Enable a timer by ID |
| `disable(id)` | Disable a timer by ID |
| `toggle(id)` | Toggle timer enabled/disabled |
| `isEnabled(id)` | Check if timer is enabled |
| `deleteTimer(id)` | Remove a timer |
| `run()` | Process timers (call in loop) |

#### Example with Timer Control

```cpp
VwireTimer timer;
int sensorTimerId;

void setup() {
  // setInterval returns timer ID for later control
  sensorTimerId = timer.setInterval(1000, readSensor);
}

VWIRE_RECEIVE(V0) {
  // Toggle sensor reading on/off from dashboard
  if (param.asBool()) {
    timer.enable(sensorTimerId);
  } else {
    timer.disable(sensorTimerId);
  }
}
```

#### Timer Slots

| Platform | Max Timers |
|----------|:----------:|
| ESP32, ESP8266 | 16 |
| AVR (Uno, Mega) | 8 |
| Other | 16 |

---

### Notifications

#### `Vwire.notify(message)`
Send push notification to mobile app.

```cpp
Vwire.notify("Temperature alert! Sensor reading: 85°C");
```

#### `Vwire.alarm(message [, sound] [, priority])`
Send persistent alarm notification with sound and vibration. **Requires PRO plan or higher.**

Triggers a persistent alarm on the mobile device that plays a looping ringtone and requires user interaction to stop. Unlike `notify()`, alarms cannot be dismissed by tapping - they require explicit STOP or SNOOZE action.

```cpp
// Basic alarm (default sound, normal priority)
Vwire.alarm("Fire detected in zone A!");

// Alarm with urgent sound
Vwire.alarm("Security breach!", "urgent");

// Alarm with warning sound and high priority
Vwire.alarm("Critical system failure!", "warning", 3);
```

**Parameters:**
- `message`: Alarm message text (required)
- `sound`: Alarm sound name (optional, default: `"default"`)
- `priority`: Priority level 1-3 (optional, default: 1)

**Available Sounds:**
| Sound | Description |
|-------|-------------|
| `"default"` | Standard alarm tone |
| `"urgent"` | High-priority urgent sound |
| `"warning"` | Moderate warning sound |

**Priority Levels:**
| Level | Description |
|:-----:|-------------|
| `1` | Normal (default) |
| `2` | High |
| `3` | Critical |

**Note:** Alarms are automatically deduplicated within 10 seconds to prevent spam. Only available for paid plans (Pro or higher).

#### `Vwire.email(subject, body)`
Send email notification.

```cpp
Vwire.email("Device Alert", "The device has detected an anomaly.");
```

#### `Vwire.log(message)`
Send log message to server.

```cpp
Vwire.log("System initialized successfully");
```

---

### Device Information

#### `Vwire.getDeviceId()`
Get the current device identifier used in MQTT topics.

- If you call `Vwire.setDeviceId(DEVICE_ID)`, this returns that Device ID (recommended).
- Otherwise it falls back to using the auth token as the topic identifier (legacy mode).

```cpp
Serial.printf("Device ID: %s\n", Vwire.getDeviceId());
```

#### `Vwire.getBoardName()`
Get board type string.

```cpp
Serial.printf("Board: %s\n", Vwire.getBoardName());
// Output: "ESP32", "ESP8266", "RP2040", etc.
```

#### `Vwire.getVersion()`
Get library version.

```cpp
Serial.printf("Library Version: %s\n", Vwire.getVersion());
// Output: "3.0.0"
```

#### `Vwire.getFreeHeap()`
Get free heap memory in bytes (ESP32/ESP8266 only).

```cpp
Serial.printf("Free Heap: %u bytes\n", Vwire.getFreeHeap());
```

#### `Vwire.getUptime()`
Get device uptime in seconds since connection.

```cpp
Serial.printf("Uptime: %u seconds\n", Vwire.getUptime());
```

#### `Vwire.getWiFiRSSI()`
Get WiFi signal strength in dBm.

```cpp
Serial.printf("WiFi Signal: %d dBm\n", Vwire.getWiFiRSSI());
```

#### `Vwire.getState()`
Get current connection state.

| State | Description |
|-------|-------------|
| `VWIRE_STATE_IDLE` | Not initialized |
| `VWIRE_STATE_CONNECTING_WIFI` | Connecting to WiFi |
| `VWIRE_STATE_CONNECTING_MQTT` | Connecting to MQTT |
| `VWIRE_STATE_CONNECTED` | Connected and ready |
| `VWIRE_STATE_DISCONNECTED` | Disconnected |
| `VWIRE_STATE_ERROR` | Error occurred |

```cpp
if (Vwire.getState() == VWIRE_STATE_CONNECTED) {
  // Ready to send data
}
```

#### `Vwire.getLastError()`
Get last error code.

| Error | Description |
|-------|-------------|
| `VWIRE_ERR_NONE` | No error |
| `VWIRE_ERR_NO_TOKEN` | Auth token not configured |
| `VWIRE_ERR_WIFI_FAILED` | WiFi connection failed |
| `VWIRE_ERR_MQTT_FAILED` | MQTT connection failed |
| `VWIRE_ERR_NOT_CONNECTED` | Not connected |
| `VWIRE_ERR_INVALID_PIN` | Invalid pin number |
| `VWIRE_ERR_BUFFER_FULL` | Buffer overflow |
| `VWIRE_ERR_HANDLER_FULL` | Max handlers reached (32) |
| `VWIRE_ERR_TIMEOUT` | Operation timeout |
| `VWIRE_ERR_SSL_FAILED` | SSL/TLS handshake failed |
| `VWIRE_ERR_QUEUE_FULL` | Reliable delivery queue full (10 msgs) |

```cpp
if (!Vwire.begin(WIFI_SSID, WIFI_PASS)) {
  VwireError err = Vwire.getLastError();
  Serial.printf("Error code: %d\n", err);
}
```

---

### OTA Updates (ESP32/ESP8266 only)

#### `Vwire.enableOTA(hostname, password)`
Enable Over-The-Air firmware updates.

```cpp
void setup() {
  Vwire.config(AUTH_TOKEN);
  Vwire.begin(WIFI_SSID, WIFI_PASS);
  
  // Enable OTA with custom hostname and password
  Vwire.enableOTA("my-esp32", "ota-password");
  
  // Or with defaults (auto hostname, no password)
  Vwire.enableOTA();
}
```

> **Note**: OTA is handled automatically in `Vwire.run()`. No additional code needed in `loop()`.

---

### Debug Functions

#### `Vwire.setDebug(enable)`
Enable/disable debug output to Serial.

```cpp
Vwire.setDebug(true);   // Enable debug messages
Vwire.setDebug(false);  // Disable debug messages
```

#### `Vwire.setDebugStream(stream)`
Set custom output stream for debug messages.

```cpp
Vwire.setDebugStream(Serial2);  // Use Serial2 for debug
```

#### `Vwire.printDebugInfo()`
Print comprehensive connection information.

```cpp
Vwire.printDebugInfo();

/* Output:
=== Vwire IOT Debug Info ===
Version: 3.0.0
Board: ESP32
Device ID: abc12345-6789-0abc-def0-123456789abc
Server: mqtt.vwire.io:8883
Transport: TLS
State: 3
WiFi RSSI: -45 dBm
Free Heap: 245632 bytes
Uptime: 3600 sec
Handlers: 3
============================
*/
```

---

## 📁 Examples

| Example | Description |
|---------|-------------|
| [01_Basic](examples/01_Basic) | Simple sensor and LED control |
| [02_ESP32_Complete](examples/02_ESP32_Complete) | ESP32 with OTA, touch, preferences |
| [03_ESP8266_Complete](examples/03_ESP8266_Complete) | ESP8266 with LittleFS storage |
| [04_DHT_SensorStation](examples/04_DHT_SensorStation) | Temperature monitoring with alerts |
| [05_Relay_Control](examples/05_Relay_Control) | Multi-relay control with buttons |
| [06_Timer](examples/06_Timer) | **VwireTimer** - Non-blocking timers |
| [07_RGB_LED_Strip](examples/07_RGB_LED_Strip) | NeoPixel/WS2812B with effects |
| [08_Motor_Servo](examples/08_Motor_Servo) | DC motor and servo control |
| [10_Minimal](examples/10_Minimal) | Simplest possible example |
| [11_MQTTS_Secure](examples/11_MQTTS_Secure) | Secure TLS connection example |
| [12_ReliableDelivery](examples/12_ReliableDelivery) | Guaranteed delivery with ACK |
| [13_SmartConfig_Provisioning](examples/13_SmartConfig_Provisioning) | 📱 WiFi provisioning via mobile app |
| [14_AP_Mode_Provisioning](examples/14_AP_Mode_Provisioning) | 📡 WiFi provisioning via Access Point |
| [15_Auto_Provisioning](examples/15_Auto_Provisioning) | ✨ **Recommended** - Automatic provisioning |

---

## 📊 Dashboard Widget Mapping

| Widget | Pin Type | Data Format | Example |
|--------|----------|-------------|---------|
| **Button** | V0-V127 | `0` or `1` | `Vwire.virtualSend(0, 1);` |
| **Slider** | V0-V127 | Number (range) | `Vwire.virtualSend(1, 75);` |
| **Gauge** | V0-V127 | Number | `Vwire.virtualSend(2, temperature);` |
| **Value Display** | V0-V127 | Number/String | `Vwire.virtualSend(3, "Online");` |
| **LED** | V0-V127 | `0` or `1` | `Vwire.virtualSend(4, ledState);` |
| **Graph** | V0-V127 | Number | `Vwire.virtualSend(5, sensorValue);` |
| **zeRGBa** | V0-V127 | `R,G,B` | `Vwire.virtualSendArray(6, rgb, 3);` |
| **Joystick** | V0-V127 | `X,Y` | Handled via array |
| **Terminal** | V0-V127 | String | `Vwire.virtualSend(7, "Log entry");` |

---

## 🔍 Troubleshooting

### Enable Debug Mode

```cpp
void setup() {
  Serial.begin(115200);
  Vwire.setDebug(true);  // Show detailed logs
  // ...
}
```

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| **WiFi won't connect** | Wrong credentials | Check SSID/password |
| **MQTT connection fails** | Wrong server/port | Verify server address and port |
| **TLS handshake fails** | Certificate issues | Use `VWIRE_TRANSPORT_TCP` for testing |
| **Frequent disconnects** | Weak WiFi signal | Check RSSI with `getWiFiRSSI()` |
| **Data not received** | Handler not defined | Define `VWIRE_RECEIVE(Vx)` handler |
| **ESP8266 crashes** | Low memory | Reduce buffer sizes, check `getFreeHeap()` |

### Memory Usage Tips (ESP8266)

```cpp
// Check available memory
Serial.printf("Free heap: %u bytes\n", Vwire.getFreeHeap());

// Typical minimum requirements:
// - Plain TCP: ~20KB free heap
// - TLS/SSL: ~40KB free heap
```

---

##  License

MIT License - see [LICENSE](LICENSE) for details.

---

## 🆘 Support

- 🌐 **Cloud Portal**: [https://vwire.io](https://vwire.io)
- 📖 **Documentation**: [https://vwire.io/docs](https://vwire.io/docs)
- 🐛 **Issues**: [https://github.com/vwireiot/vwire-arduino/issues](https://github.com/vwireiot/vwire-arduino/issues)
- 📧 **Email**: support@vwire.io

---

**Made with ❤️ by Vwire IOT Team**
