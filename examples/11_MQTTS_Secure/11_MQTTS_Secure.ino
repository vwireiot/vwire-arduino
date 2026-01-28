/*
 * Vwire IOT - Secure MQTTS Example
 * 
 * Demonstrates secure MQTT connection over TLS (port 8883).
 * This is the RECOMMENDED transport for all deployments.
 * 
 * TLS provides:
 * - Encrypted data transmission
 * - Server authentication
 * - Protection against eavesdropping
 * 
 * Hardware:
 * - ESP32 or ESP8266 board
 * - LED on GPIO2 (built-in on most boards)
 * 
 * Requirements:
 * - Vwire IOT library
 * - Server with MQTTS enabled (port 8883)
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>

// ============================================================================
// CONFIGURATION - Update these values!
// ============================================================================
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define AUTH_TOKEN    "your_device_auth_token"

// Transport: VWIRE_TRANSPORT_TCP_SSL (port 8883) - TLS encrypted ✅ RECOMMENDED
//           VWIRE_TRANSPORT_TCP     (port 1883) - Plain (for boards without SSL)
#define TRANSPORT     VWIRE_TRANSPORT_TCP_SSL

// ============================================================================
// PINS
// ============================================================================
#define LED_PIN       2  // Built-in LED (active LOW on most ESP8266)
#define BUTTON_PIN    0  // Flash button on most ESP boards

// Virtual pins for Vwire dashboard
// V0 = LED control from dashboard
// V1 = Button state to dashboard
// V2 = Free heap memory (for monitoring)
// V3 = Device uptime

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
unsigned long lastHeapReport = 0;
bool ledState = false;

// ============================================================================
// VWIRE HANDLERS
// ============================================================================

// Called when dashboard sends a value to V0 (LED)
VWIRE_RECEIVE(V0) {
  ledState = param.asBool();
  #if defined(ESP8266)
  digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // Active LOW on ESP8266
  #else
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  #endif
  Serial.printf("LED set to: %s\n", ledState ? "ON" : "OFF");
}

// Called when Vwire connects to the broker
VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  Serial.printf("  Transport: MQTTS (TLS)\n");
  Serial.printf("  Server: mqtt.vwire.io:8883\n");
  Serial.printf("  Free heap: %d bytes\n", Vwire.getFreeHeap());
  
  // Sync current LED state to dashboard
  Vwire.virtualSend(V0, ledState);
}

// Called when Vwire disconnects
VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected from Vwire IOT!");
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("  Vwire IOT MQTTS Secure Example");
  Serial.println("=================================\n");
  
  // Print initial heap
  #if defined(ESP32) || defined(ESP8266)
  Serial.printf("Initial free heap: %d bytes\n", ESP.getFreeHeap());
  #endif
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  #if defined(ESP8266)
  digitalWrite(LED_PIN, HIGH);  // LED off (active LOW on ESP8266)
  #else
  digitalWrite(LED_PIN, LOW);
  #endif
  
  // Configure Vwire (uses default server mqtt.vwire.io)
  Vwire.config(AUTH_TOKEN);  // Uses default server and port
  Vwire.setTransport(TRANSPORT);
  Vwire.setDebug(true);
  
  // Note: VWIRE_RECEIVE, VWIRE_CONNECTED, VWIRE_DISCONNECTED macros auto-register!
  
  // Connect to WiFi and MQTT broker
  Serial.println("Connecting...");
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
  
  #if defined(ESP32) || defined(ESP8266)
  Serial.printf("After connection - Free heap: %d bytes\n", ESP.getFreeHeap());
  #endif
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  Vwire.run();
  
  // Read button (debounced)
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounce = 0;
  bool buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState != lastButtonState && millis() - lastDebounce > 50) {
    lastDebounce = millis();
    lastButtonState = buttonState;
    
    if (buttonState == LOW) {  // Button pressed
      Serial.println("Button pressed!");
      Vwire.virtualSend(V1, 1);
      Vwire.notify("Button pressed on device!");
    } else {
      Vwire.virtualSend(V1, 0);
    }
  }
  
  // Report heap and uptime every 30 seconds
  if (Vwire.connected() && millis() - lastHeapReport > 30000) {
    lastHeapReport = millis();
    
    Vwire.virtualSend(V2, Vwire.getFreeHeap());
    Vwire.virtualSend(V3, Vwire.getUptime());
    
    Serial.printf("Status: Heap=%u bytes, Uptime=%u sec, RSSI=%d dBm\n",
                  Vwire.getFreeHeap(), Vwire.getUptime(), Vwire.getWiFiRSSI());
  }
}
