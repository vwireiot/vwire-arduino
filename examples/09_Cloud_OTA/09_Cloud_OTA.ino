/*
 * Vwire IOT - Cloud OTA Firmware Update Example
 * 
 * This example demonstrates over-the-air firmware updates
 * from the Vwire IOT cloud platform.
 * 
 * Supported Boards: ESP32, ESP8266
 * 
 * How Cloud OTA Works:
 * 1. Device connects to Vwire platform and reports OTA capability
 * 2. You upload firmware binary (.bin) from the dashboard
 * 3. Server sends OTA command to device via MQTT
 * 4. Device downloads the firmware over HTTP and installs it
 * 5. Device reboots with the new firmware
 * 
 * Dashboard Usage:
 * - Open device detail page
 * - Expand "OTA Firmware Update" section
 * - Click "Upload & Deploy" to upload a .bin file
 * - Monitor progress in real-time
 * 
 * Building a .bin File:
 * - Arduino IDE: Sketch > Export Compiled Binary
 * - PlatformIO: pio run (finds .bin in .pio/build/<env>/)
 * 
 * Instructions:
 * 1. Create device in Vwire IOT dashboard
 * 2. Copy auth token and device ID below
 * 3. Update WiFi credentials
 * 4. Upload this sketch to your board
 * 5. Use dashboard to deploy firmware updates
 * 
 * Note: Cloud OTA is automatically enabled on ESP32/ESP8266.
 *       To disable, define VWIRE_DISABLE_CLOUD_OTA before including Vwire.h.
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
const char* AUTH_TOKEN = "YOUR_AUTH_TOKEN";
const char* DEVICE_ID = "YOUR_DEVICE_ID";  // VW-XXXXXX or VU-XXXXXX

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// =============================================================================
// VARIABLES
// =============================================================================
unsigned long lastSend = 0;
int counter = 0;

// =============================================================================
// RECEIVE: Dashboard button controls LED (V0)
// =============================================================================
VWIRE_RECEIVE(V0) {
  int value = param.asInt();
  digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  Serial.printf("LED: %s\n", value ? "ON" : "OFF");
}

// =============================================================================
// CONNECTION HANDLER
// =============================================================================
VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  Serial.printf("Firmware version: %s\n", VWIRE_VERSION);
  
  #if VWIRE_ENABLE_CLOUD_OTA
  Serial.println("Cloud OTA: ENABLED");
  #else
  Serial.println("Cloud OTA: DISABLED (board not supported)");
  #endif
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
  
  Serial.println("\n=== Vwire IOT Cloud OTA Example ===");
  Serial.printf("Library version: %s\n", VWIRE_VERSION);
  Serial.printf("Board: %s\n", VWIRE_BOARD_NAME);
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Configure Vwire
  Vwire.setAuthToken(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);
  Vwire.setTransport(TRANSPORT);
  Vwire.setDebug(true);
  
  // Enable Cloud OTA - device will accept firmware updates from dashboard
  #if VWIRE_ENABLE_CLOUD_OTA
  Vwire.enableCloudOTA();
  Serial.println("Cloud OTA enabled - device will accept firmware updates");
  #endif
  
  // Optional: Also enable local network OTA (Arduino IDE OTA)
  #if VWIRE_HAS_OTA
  // Vwire.enableOTA();  // Uncomment to also allow Arduino IDE OTA
  #endif
  
  // Connect to WiFi and MQTT
  if (Vwire.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("Connected successfully!");
  } else {
    Serial.println("Connection failed!");
  }
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  Vwire.run();
  
  // Send data every 5 seconds
  if (millis() - lastSend >= 5000) {
    lastSend = millis();
    counter++;
    
    // Send uptime to V1 (Gauge widget)
    Vwire.virtualSend(V1, Vwire.getUptime());
    
    // Send counter to V2 (Value display)
    Vwire.virtualSend(V2, counter);
    
    // Send free heap to V3 (for monitoring)
    Vwire.virtualSend(V3, Vwire.getFreeHeap());
    
    Serial.printf("Sent: uptime=%lu counter=%d heap=%lu\n", 
      Vwire.getUptime(), counter, Vwire.getFreeHeap());
  }
}
