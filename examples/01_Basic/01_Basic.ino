/*
 * Vwire IOT - Basic Example
 * 
 * This example works with any WiFi-capable board:
 * - ESP32, ESP8266, Arduino WiFi, RP2040-W, etc.
 * 
 * Demonstrates:
 * - Connecting to Vwire IOT platform via MQTTS (TLS)
 * - Sending sensor data to dashboard
 * - Receiving button press from dashboard
 * - LED control using VWIRE_RECEIVE()
 * 
 * Dashboard Setup:
 * - V0: Button widget (controls LED)
 * - V1: Gauge widget (displays temperature)
 * - V2: Value display (shows counter)
 * 
 * Instructions:
 * 1. Create device in Vwire IOT dashboard
 * 2. Copy auth token below
 * 3. Update WiFi credentials
 * 4. Update MQTT broker settings (if self-hosted)
 * 5. Upload to your board
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>

// =============================================================================
// WIFI CONFIGURATION
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// =============================================================================
// VWIRE IOT AUTHENTICATION
// =============================================================================
const char* AUTH_TOKEN    = "YOUR_AUTH_TOKEN";
const char* DEVICE_ID     = "YOUR_DEVICE_ID";  // VW-XXXXXX (OEM) or VU-XXXXXX (user-created)

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED for most boards
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, use if board doesn't support SSL
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2  // Default for ESP32
#endif

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
bool ledState = false;
int counter = 0;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 2000;  // Send data every 2 seconds

// =============================================================================
// VIRTUAL PIN HANDLERS
// =============================================================================

// V0 - Button widget handler
// Automatically registered - no need to call Vwire.onVirtualReceive()!
VWIRE_RECEIVE(V0) {
  ledState = param.asBool();
  digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  
  Serial.print("LED: ");
  Serial.println(ledState ? "ON" : "OFF");
}

// =============================================================================
// CONNECTION HANDLERS 
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  
  // Request stored values from server (useful after power cycle)
  Vwire.sync(V0);  // Sync LED state
  
  // Or sync multiple pins at once:
  // Vwire.sync(V0, V1, V2);
  // Vwire.syncAll();  // Sync all pins
  
  // Print connection info
  Vwire.printDebugInfo();
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected from Vwire IOT!");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - Basic Example");
  Serial.println("================================");
  Serial.println();
  
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Enable debug output
  Vwire.setDebug(true);
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics (recommended)
  Vwire.setTransport(TRANSPORT);
  
  // No need to register handlers manually!
  // VWIRE_RECEIVE(V0), VWIRE_CONNECTED(), VWIRE_DISCONNECTED() 
  // are auto-registered at startup.
  
  // Connect to WiFi and Vwire
  Serial.println("Connecting...");
  if (Vwire.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("Connection successful!");
  } else {
    Serial.println("Connection failed!");
    Serial.print("Error code: ");
    Serial.println(Vwire.getLastError());
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // IMPORTANT: Must call run() frequently
  Vwire.run();
  
  // Send sensor data periodically
  if (Vwire.connected() && millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    
    // Simulate temperature reading (replace with actual sensor)
    float temperature = 20.0 + random(0, 100) / 10.0;
    
    // Increment counter
    counter++;
    
    // Send to dashboard
    Vwire.virtualSend(V1, temperature);  // V1 = Temperature gauge
    Vwire.virtualSend(V2, counter);      // V2 = Counter display
    
    // Print to serial
    Serial.printf("Sent: Temp=%.1f°C, Counter=%d\n", temperature, counter);
  }
}
