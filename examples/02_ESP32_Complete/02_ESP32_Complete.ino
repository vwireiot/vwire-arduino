/*
 * Vwire IOT - ESP32 Complete Example
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: ESP32 (all variants)
 * 
 * Demonstrates ESP32-specific features:
 * - Dual-core task handling
 * - OTA (Over-The-Air) updates
 * - Deep sleep with wake on timer/GPIO
 * - Touch sensor input
 * - Multiple ADC channels
 * - PWM LED control
 * - NVS preferences storage
 * 
 * Dashboard Setup:
 * - V0: Button (LED on/off)
 * - V1: Slider (LED brightness, 0-255)
 * - V2: Gauge (Temperature)
 * - V3: Gauge (Humidity)
 * - V4: Value (Touch sensor)
 * - V5: Value (Battery voltage)
 * - V6: LED (Online status)
 * - V7: Terminal (Logs)
 * 
 * Hardware:
 * - Built-in LED or external LED on GPIO 2
 * - Touch pin on GPIO 4
 * - Battery voltage divider on GPIO 34
 * - DHT22 on GPIO 15 (optional)
 */

#include <Vwire.h>
#include <Preferences.h>

// =============================================================================
// WIFI CONFIGURATION
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// =============================================================================
// VWIRE IOT AUTHENTICATION
// =============================================================================
const char* AUTH_TOKEN    = "YOUR_AUTH_TOKEN";

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, use if SSL not supported
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#define LED_PIN           2
#define TOUCH_PIN         4
#define BATTERY_PIN       34
#define DHT_PIN           15

// PWM Configuration
#define PWM_CHANNEL       0
#define PWM_FREQ          5000
#define PWM_RESOLUTION    8

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
Preferences preferences;

bool ledState = false;
int ledBrightness = 128;
int touchThreshold = 40;
float batteryVoltage = 0;
float temperature = 0;
float humidity = 0;

unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
const unsigned long SENSOR_READ_INTERVAL = 1000;
const unsigned long DATA_SEND_INTERVAL = 5000;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================
void readSensors();
void sendSensorData();
void updateLED();
void saveSettings();
void loadSettings();

// =============================================================================
// VIRTUAL PIN HANDLERS 
// =============================================================================

// V0 - LED On/Off button
VWIRE_RECEIVE(V0) {
  ledState = param.asBool();
  updateLED();
  saveSettings();
  
  // Log to terminal widget
  Vwire.virtualSendf(V7, "LED %s", ledState ? "ON" : "OFF");
  Serial.printf("LED: %s\n", ledState ? "ON" : "OFF");
}

// V1 - LED brightness slider (0-255)
VWIRE_RECEIVE(V1) {
  ledBrightness = param.asInt();
  updateLED();
  saveSettings();
  
  Vwire.virtualSendf(V7, "Brightness: %d", ledBrightness);
  Serial.printf("Brightness: %d\n", ledBrightness);
}

// =============================================================================
// SENSOR FUNCTIONS
// =============================================================================

void readSensors() {
  // Read touch sensor
  int touchValue = touchRead(TOUCH_PIN);
  
  // Read battery voltage (assuming voltage divider: Vbat -> 100K -> GPIO -> 100K -> GND)
  int adcValue = analogRead(BATTERY_PIN);
  batteryVoltage = (adcValue / 4095.0) * 3.3 * 2;  // *2 for voltage divider
  
  // Simulate DHT22 (replace with actual sensor library)
  temperature = 22.0 + random(-50, 50) / 10.0;
  humidity = 50.0 + random(-100, 100) / 10.0;
  
  // Check touch threshold
  static bool lastTouchState = false;
  bool touched = touchValue < touchThreshold;
  
  if (touched && !lastTouchState) {
    Serial.println("Touch detected!");
    Vwire.notify("Touch sensor activated!");
    
    // Toggle LED on touch
    ledState = !ledState;
    updateLED();
    Vwire.virtualSend(V0, ledState);
  }
  lastTouchState = touched;
}

void sendSensorData() {
  Vwire.virtualSend(V2, temperature);        // Temperature
  Vwire.virtualSend(V3, humidity);           // Humidity
  Vwire.virtualSend(V4, touchRead(TOUCH_PIN)); // Touch value
  Vwire.virtualSend(V5, batteryVoltage);     // Battery voltage
  Vwire.virtualSend(V6, 1);                  // Online indicator
  
  Serial.printf("Sent: T=%.1f°C H=%.1f%% Bat=%.2fV\n", 
                temperature, humidity, batteryVoltage);
}

void updateLED() {
  if (ledState) {
    ledcWrite(PWM_CHANNEL, ledBrightness);
  } else {
    ledcWrite(PWM_CHANNEL, 0);
  }
}

// =============================================================================
// SETTINGS PERSISTENCE
// =============================================================================

void saveSettings() {
  preferences.begin("vwire", false);
  preferences.putBool("ledState", ledState);
  preferences.putInt("brightness", ledBrightness);
  preferences.end();
}

void loadSettings() {
  preferences.begin("vwire", true);
  ledState = preferences.getBool("ledState", false);
  ledBrightness = preferences.getInt("brightness", 128);
  preferences.end();
  
  Serial.printf("Loaded: LED=%s, Brightness=%d\n", 
                ledState ? "ON" : "OFF", ledBrightness);
}

// =============================================================================
// CONNECTION HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  
  // Request stored values from server
  Vwire.sync(V0, V1);  // Sync LED state and brightness
  
  // Send online status
  Vwire.virtualSend(V6, 1);  // Online
  
  // Send startup log
  Vwire.virtualSend(V7, "ESP32 connected!");
  Vwire.virtualSendf(V7, "Free heap: %d bytes", ESP.getFreeHeap());
  
  Vwire.printDebugInfo();
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected!");
  // LED indicator for offline
  ledcWrite(PWM_CHANNEL, 0);
  delay(100);
  ledcWrite(PWM_CHANNEL, 50);
  delay(100);
  ledcWrite(PWM_CHANNEL, 0);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - ESP32 Complete");
  Serial.println("================================");
  Serial.printf("  Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("  CPU: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("  Flash: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
  Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("================================\n");
  
  // Load saved settings
  loadSettings();
  
  // Configure PWM for LED
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
  updateLED();
  
  // Configure touch pin
  touchSetCycles(0x1000, 0x1000);
  
  // Configure ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  // No need to register handlers - VWIRE_RECEIVE() macros auto-register!
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setTransport(TRANSPORT);
  
  // Enable OTA updates
  Vwire.enableOTA("vwire-esp32");
  
  // Connect
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
  
  // Read sensors
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = millis();
    readSensors();
  }
  
  // Send data to dashboard
  if (Vwire.connected() && millis() - lastDataSend >= DATA_SEND_INTERVAL) {
    lastDataSend = millis();
    sendSensorData();
  }
}

// =============================================================================
// DEEP SLEEP EXAMPLE (uncomment to use)
// =============================================================================
/*
void enterDeepSleep(int seconds) {
  Serial.printf("Entering deep sleep for %d seconds...\n", seconds);
  
  // Save state before sleep
  saveSettings();
  
  // Disconnect cleanly
  Vwire.disconnect();
  
  // Configure wake sources
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_sleep_enable_touchpad_wakeup();
  
  // Enter deep sleep
  esp_deep_sleep_start();
}
*/
