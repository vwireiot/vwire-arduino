/*
 * Vwire IOT - Timer Example
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: Any WiFi-capable board (ESP32, ESP8266, etc.)
 * 
 * Demonstrates VwireTimer features:
 * - setInterval: Repeating timer
 * - setTimeout: One-shot timer
 * - setTimer: Run N times
 * - Timer control (enable/disable/toggle)
 * - Callbacks with arguments
 * 
 * VwireTimer uses millis() internally, making it compatible
 * with ALL Arduino boards including AVR (Uno, Nano, Mega).
 * 
 * Dashboard Setup:
 * - V0: Gauge (WiFi signal strength)
 * - V1: Value (Temperature)
 * - V2: Value (Uptime seconds)
 * - V3: LED (Heartbeat indicator)
 * - V4: Button (Toggle fast blink)
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
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, use if SSL not supported
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#define LED_PIN LED_BUILTIN

// =============================================================================
// TIMER INSTANCE
// =============================================================================
VwireTimer timer;

// Timer IDs for control
int rssiTimerId = -1;
int sensorTimerId = -1;
int blinkTimerId = -1;
int uptimeTimerId = -1;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
unsigned long uptimeSeconds = 0;
bool ledState = false;

// =============================================================================
// TIMER CALLBACKS
// =============================================================================

/**
 * Send WiFi RSSI every second
 */
void sendRSSI() {
  int rssi = WiFi.RSSI();
  // Convert to percentage
  int quality = 0;
  if (rssi <= -100) quality = 0;
  else if (rssi >= -50) quality = 100;
  else quality = 2 * (rssi + 100);
  
  Vwire.virtualSend(V0, quality);
  Serial.printf("RSSI: %d dBm (%d%%)\n", rssi, quality);
}

/**
 * Read and send sensor data every 5 seconds
 */
void sendSensorData() {
  // Simulated temperature (replace with real sensor)
  float temperature = 22.0 + (random(-20, 20) / 10.0);
  
  Vwire.virtualSend(V1, temperature);
  Serial.printf("Temperature: %.1f°C\n", temperature);
}

/**
 * Blink LED - toggle state
 */
void blinkLED() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  
  // Send heartbeat to dashboard
  Vwire.virtualSend(V3, ledState ? 1 : 0);
}

/**
 * Update uptime counter every second
 */
void updateUptime() {
  uptimeSeconds++;
  Vwire.virtualSend(V2, uptimeSeconds);
}

/**
 * Example: Callback with argument
 * Pin number is passed as argument
 */
void togglePin(void* arg) {
  int pin = (int)(intptr_t)arg;
  digitalWrite(pin, !digitalRead(pin));
  Serial.printf("Toggled pin %d\n", pin);
}

/**
 * One-shot startup notification
 */
void startupComplete() {
  Serial.println(">>> Startup sequence complete!");
  Vwire.notify("Device started successfully!");
}

// =============================================================================
// VIRTUAL PIN HANDLERS
// =============================================================================

// Toggle fast blink mode
VWIRE_RECEIVE(V4) {
  if (param.asBool()) {
    // Change to fast blink (100ms)
    timer.changeInterval(blinkTimerId, 100);
    Serial.println("Fast blink enabled");
  } else {
    // Change to slow blink (1000ms)
    timer.changeInterval(blinkTimerId, 1000);
    Serial.println("Normal blink restored");
  }
}

// =============================================================================
// CONNECTION HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Enable all timers when connected
  timer.enable(rssiTimerId);
  timer.enable(sensorTimerId);
  timer.enable(uptimeTimerId);
  
  // Print timer status
  Serial.printf("Active timers: %d / %d\n", 
                timer.getNumTimers(), 
                timer.getMaxTimers());
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
  
  // Optionally disable some timers when disconnected
  // timer.disable(rssiTimerId);
  // timer.disable(sensorTimerId);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - Timer Example");
  Serial.println("================================");
  Serial.printf("  Max timers: %d\n", timer.getMaxTimers());
  Serial.println("================================\n");
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // =========================================================================
  // SETUP TIMERS
  // =========================================================================
  
  // 1. Repeating interval - send RSSI every 1 second
  rssiTimerId = timer.setInterval(1000L, sendRSSI);
  Serial.printf("Created RSSI timer: ID=%d\n", rssiTimerId);
  
  // 2. Repeating interval - send sensor data every 5 seconds
  sensorTimerId = timer.setInterval(5000L, sendSensorData);
  Serial.printf("Created sensor timer: ID=%d\n", sensorTimerId);
  
  // 3. Repeating interval - blink LED every 1 second
  blinkTimerId = timer.setInterval(1000L, blinkLED);
  Serial.printf("Created blink timer: ID=%d\n", blinkTimerId);
  
  // 4. Repeating interval - update uptime every second
  uptimeTimerId = timer.setInterval(1000L, updateUptime);
  Serial.printf("Created uptime timer: ID=%d\n", uptimeTimerId);
  
  // 5. One-shot timeout - notify after 10 seconds
  int startupTimerId = timer.setTimeout(10000L, startupComplete);
  Serial.printf("Created startup timer: ID=%d (one-shot)\n", startupTimerId);
  
  // 6. Run N times example - blink 5 times rapidly at startup
  int welcomeBlinkId = timer.setTimer(200L, blinkLED, 10);  // 10 toggles = 5 blinks
  Serial.printf("Created welcome blink: ID=%d (runs 10 times)\n", welcomeBlinkId);
  
  // 7. Example with argument - toggle a specific pin
  // int pinToggleId = timer.setInterval(2000L, togglePin, (void*)(intptr_t)LED_PIN);
  
  Serial.printf("\nTotal timers created: %d\n", timer.getNumTimers());
  Serial.printf("Available slots: %d\n\n", timer.getNumAvailableTimers());
  
  // =========================================================================
  // CONNECT TO VWIRE IOT
  // =========================================================================
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  Serial.println("Connecting to WiFi and Vwire IOT...");
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // Run Vwire (handles MQTT, OTA, reconnection)
  Vwire.run();
  
  // Run all timers - MUST be called in loop()
  timer.run();
  
  // Note: No delay() needed! Timers are non-blocking.
}
