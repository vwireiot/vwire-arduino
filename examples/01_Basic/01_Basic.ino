/*
 * Vwire IOT - Basic Example
 * 
 * Works with any WiFi-capable board:
 * - ESP32, ESP8266, Arduino WiFi, RP2040-W, etc.
 * 
 * Demonstrates:
 * - Connecting to Vwire IOT platform via MQTTS (TLS)
 * - LED control using VWIRE_RECEIVE() on a virtual pin
 * - Enabling cloud-managed GPIO support
 * - Syncing stored pin values on connect
 *
 * Dashboard Setup:
 * - V0: Button/Switch widget (controls built-in LED)
 * - For GPIO pin control, see examples 15_Digital_Pins and 16_Analog_Pins
 * 
 * Instructions:
 * 1. Create device in Vwire IOT dashboard
 * 2. Copy auth token and device ID below
 * 3. Update WiFi credentials
 * 4. Upload to your board
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>

// =============================================================================
// CONFIGURATION — UPDATE THESE
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* AUTH_TOKEN    = "YOUR_AUTH_TOKEN";
const char* DEVICE_ID     = "YOUR_DEVICE_ID";  // VW-XXXXXX or VU-XXXXXX

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2  // Default for ESP32
#endif

// =============================================================================
// VIRTUAL PIN HANDLERS
// =============================================================================

// V0 — Button/Switch widget controls the built-in LED
VWIRE_RECEIVE(V0) {
  bool on = param.asBool();
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
  Serial.print("LED: ");
  Serial.println(on ? "ON" : "OFF");
}

// =============================================================================
// CONNECTION HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  // Sync only the LED state on connect (optional)
  // Vwire.sync(V0);  // Restore LED state after power cycle
  // Sync all pins with stored values on connect (optional)
  Vwire.syncAll();
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected from Vwire IOT!");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Optional logging:
  // Vwire.logTo(Serial);  // Recommended: print library logs to Serial
  // Vwire.onLog([](const char* msg) { Serial.println(msg); });
  // Vwire.disableLog();   // Silent mode (default)
  
  // Single-call config: auth token + device ID
  // TLS is the default transport
  Vwire.config(AUTH_TOKEN, DEVICE_ID);

  // Enable cloud-managed GPIO support.
  // This lets you use D*/A* pins from the dashboard when needed.
  Vwire.enableGPIO();
  
  // Connect to WiFi and Vwire
  if (Vwire.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("Connection successful!");
  } else {
    Serial.println("Connection failed!");
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
}
