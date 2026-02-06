/*
 * Vwire IOT - ESP8266 Complete Example
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: ESP8266 (NodeMCU, Wemos D1, ESP-01, etc.)
 * 
 * Demonstrates ESP8266-specific features:
 * - OTA updates
 * - Web configuration portal
 * - LittleFS for settings storage
 * - Watchdog timer
 * - ADC reading
 * - PWM on any GPIO
 * 
 * Dashboard Setup:
 * - V0: Button (LED control)
 * - V1: Slider (LED brightness, 0-1023)
 * - V2: Gauge (Temperature)
 * - V3: Gauge (Light level)
 * - V4: Value (WiFi signal)
 * - V5: LED (Online status)
 * - V6: Graph (Temperature history)
 * 
 * Hardware:
 * - Built-in LED on GPIO 2 (inverted on most boards)
 * - LDR on A0
 * - DS18B20 or DHT on GPIO 4 (optional)
 */

#include <Vwire.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

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
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED for ESP8266
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, use if SSL not supported
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS (NodeMCU mapping)
// =============================================================================
#define LED_PIN       LED_BUILTIN  // GPIO 2, inverted
#define LDR_PIN       A0           // Analog input
#define SENSOR_PIN    D2           // GPIO 4

// Note: ESP8266 LED_BUILTIN is inverted (LOW = ON)
#define LED_ON  LOW
#define LED_OFF HIGH

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
bool ledState = false;
int ledBrightness = 512;
float temperature = 0;
int lightLevel = 0;

unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long SEND_INTERVAL = 5000;

// =============================================================================
// SETTINGS FILE
// =============================================================================
const char* SETTINGS_FILE = "/settings.json";

void saveSettings() {
  StaticJsonDocument<200> doc;
  doc["ledState"] = ledState;
  doc["brightness"] = ledBrightness;
  
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Settings saved");
  }
}

void loadSettings() {
  if (LittleFS.exists(SETTINGS_FILE)) {
    File file = LittleFS.open(SETTINGS_FILE, "r");
    if (file) {
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, file);
      file.close();
      
      if (!error) {
        ledState = doc["ledState"] | false;
        ledBrightness = doc["brightness"] | 512;
        Serial.printf("Settings loaded: LED=%d, Brightness=%d\n", 
                      ledState, ledBrightness);
      }
    }
  }
}

// =============================================================================
// LED CONTROL
// =============================================================================

void updateLED() {
  if (ledState) {
    // ESP8266 PWM is 0-1023 by default
    // Invert for built-in LED
    analogWrite(LED_PIN, 1023 - ledBrightness);
  } else {
    analogWrite(LED_PIN, 1023);  // OFF (inverted)
  }
}

// =============================================================================
// VIRTUAL PIN HANDLERS
// =============================================================================

VWIRE_RECEIVE(V0) {
  ledState = param.asBool();
  updateLED();
  saveSettings();
  Serial.printf("LED: %s\n", ledState ? "ON" : "OFF");
}

VWIRE_RECEIVE(V1) {
  ledBrightness = param.asInt();
  updateLED();
  saveSettings();
  Serial.printf("Brightness: %d\n", ledBrightness);
}

// =============================================================================
// SENSOR FUNCTIONS
// =============================================================================

void readSensors() {
  // Read light level from LDR (0-1023)
  lightLevel = analogRead(LDR_PIN);
  
  // Convert to percentage (assuming higher value = more light)
  int lightPercent = map(lightLevel, 0, 1023, 0, 100);
  
  // Simulate temperature (replace with DS18B20 or DHT library)
  temperature = 22.0 + (random(-30, 30) / 10.0);
}

void sendSensorData() {
  Vwire.virtualSend(V2, temperature);
  Vwire.virtualSend(V3, map(lightLevel, 0, 1023, 0, 100));
  Vwire.virtualSend(V4, WiFi.RSSI());
  Vwire.virtualSend(V5, 1);  // Online
  Vwire.virtualSend(V6, temperature);  // For graph
  
  Serial.printf("Sent: T=%.1f°C, Light=%d%%, RSSI=%d\n",
                temperature, 
                map(lightLevel, 0, 1023, 0, 100),
                WiFi.RSSI());
}

// =============================================================================
// CONNECTION HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  
  // Request stored values from server
  Vwire.sync(V0, V1);  // Sync LED state and brightness
  
  // Send online status
  Vwire.virtualSend(V5, 1);
  
  // Print debug info
  Serial.printf("Chip ID: %08X\n", ESP.getChipId());
  Serial.printf("Flash Size: %d KB\n", ESP.getFlashChipSize() / 1024);
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
  
  Vwire.printDebugInfo();
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected!");
  
  // Blink LED to indicate disconnection
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LED_ON);
    delay(100);
    digitalWrite(LED_PIN, LED_OFF);
    delay(100);
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - ESP8266 Complete");
  Serial.println("================================");
  Serial.printf("  Chip ID: %08X\n", ESP.getChipId());
  Serial.printf("  CPU: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("  Flash: %d KB\n", ESP.getFlashChipSize() / 1024);
  Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("================================\n");
  
  // Initialize file system
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }
  
  // Load saved settings
  loadSettings();
  
  // Configure LED pin
  pinMode(LED_PIN, OUTPUT);
  analogWriteRange(1023);  // Set PWM range
  analogWriteFreq(1000);   // Set PWM frequency
  updateLED();
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  // No need to register handlers - VWIRE_RECEIVE() macros auto-register!
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  // Enable OTA updates
  Vwire.enableOTA("vwire-esp8266");
  
  // Connect
  Serial.println("Connecting to WiFi and Vwire IOT...");
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // Feed watchdog
  yield();
  
  // Run Vwire
  Vwire.run();
  
  // Read sensors
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    readSensors();
  }
  
  // Send data
  if (Vwire.connected() && millis() - lastDataSend >= SEND_INTERVAL) {
    lastDataSend = millis();
    sendSensorData();
  }
  
  // Memory monitoring (ESP8266 can run low on heap)
  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck >= 30000) {
    lastHeapCheck = millis();
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    // Restart if heap is too low
    if (ESP.getFreeHeap() < 5000) {
      Serial.println("Low memory, restarting...");
      ESP.restart();
    }
  }
}

// =============================================================================
// OPTIONAL: WiFi Manager for configuration portal
// =============================================================================
/*
#include <WiFiManager.h>

void setupWiFiManager() {
  WiFiManager wm;
  
  // Custom parameters
  WiFiManagerParameter custom_token("token", "Vwire Token", AUTH_TOKEN, 64);
  wm.addParameter(&custom_token);
  
  // Auto connect or start config portal
  if (!wm.autoConnect("Vwire-Setup")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }
  
  // Save token if changed
  // strcpy(AUTH_TOKEN, custom_token.getValue());
}
*/
