/*
 * Vwire IOT - DHT Sensor Station
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: Any WiFi-capable board (ESP32, ESP8266, etc.)
 * 
 * A complete environmental monitoring station with:
 * - Temperature and humidity monitoring
 * - Configurable alert thresholds
 * - Historical data graphing
 * - Push notifications on threshold breach
 * - Email daily summary
 * 
 * Dashboard Setup:
 * - V0: Gauge (Temperature, 0-50C)
 * - V1: Gauge (Humidity, 0-100%)
 * - V2: Value (Heat Index)
 * - V3: SuperChart (Temperature history)
 * - V4: SuperChart (Humidity history)
 * - V5: Slider (Temp alert threshold)
 * - V6: LED (Alert indicator)
 * - V7: Terminal (Event log)
 * 
 * Hardware:
 * - DHT22 sensor on GPIO 4
 * - Status LED on GPIO 2
 */

#include <Vwire.h>
#include <DHT.h>

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
// DHT SENSOR CONFIG
// =============================================================================
#define DHT_PIN  4
#define DHT_TYPE DHT22  // DHT11 or DHT22

// Uncomment if using DHT11
// #define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// =============================================================================
// LED PIN
// =============================================================================
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
float temperature = 0;
float humidity = 0;
float heatIndex = 0;
float tempAlertThreshold = 30.0;

bool alertActive = false;
int consecutiveAlerts = 0;
const int ALERT_COOLDOWN = 3;  // Require 3 consecutive readings before alert

// Statistics
float tempMin = 100, tempMax = -100;
float humMin = 100, humMax = 0;
int readingCount = 0;

// Timing
unsigned long lastRead = 0;
unsigned long lastSend = 0;
unsigned long lastDailySummary = 0;

const unsigned long READ_INTERVAL = 2000;   // Read every 2 seconds
const unsigned long SEND_INTERVAL = 10000;  // Send every 10 seconds
const unsigned long DAILY_INTERVAL = 86400000; // 24 hours

// =============================================================================
// SENSOR FUNCTIONS
// =============================================================================

bool readDHT() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // Check if reading failed
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT read failed!");
    return false;
  }
  
  temperature = t;
  humidity = h;
  
  // Calculate heat index
  heatIndex = dht.computeHeatIndex(t, h, false);
  
  // Update statistics
  if (t < tempMin) tempMin = t;
  if (t > tempMax) tempMax = t;
  if (h < humMin) humMin = h;
  if (h > humMax) humMax = h;
  readingCount++;
  
  return true;
}

void checkAlerts() {
  if (temperature > tempAlertThreshold) {
    consecutiveAlerts++;
    
    if (consecutiveAlerts >= ALERT_COOLDOWN && !alertActive) {
      alertActive = true;
      
      // Send notification
      char msg[100];
      snprintf(msg, sizeof(msg), 
               "High temperature alert! Current: %.1fC (Threshold: %.1fC)", 
               temperature, tempAlertThreshold);
      Vwire.notify(msg);
      
      // Log to terminal
      Vwire.virtualSend(V7, msg);
      
      // Turn on alert LED
      Vwire.virtualSend(V6, 1);
      digitalWrite(LED_BUILTIN, HIGH);
      
      Serial.println(msg);
    }
  } else {
    if (consecutiveAlerts > 0) consecutiveAlerts--;
    
    if (consecutiveAlerts == 0 && alertActive) {
      alertActive = false;
      Vwire.virtualSend(V6, 0);
      digitalWrite(LED_BUILTIN, LOW);
      Vwire.virtualSend(V7, "Temperature returned to normal");
      Serial.println("Alert cleared");
    }
  }
}

void sendSensorData() {
  // Send current values
  Vwire.virtualSend(V0, temperature);
  Vwire.virtualSend(V1, humidity);
  Vwire.virtualSend(V2, heatIndex);
  
  // Send to graphs (SuperChart widget)
  Vwire.virtualSend(V3, temperature);
  Vwire.virtualSend(V4, humidity);
  
  Serial.printf("Sent: T=%.1fC, H=%.1f%%, HI=%.1fC\n",
                temperature, humidity, heatIndex);
}

void sendDailySummary() {
  if (readingCount == 0) return;
  
  char subject[64];
  snprintf(subject, sizeof(subject), "Vwire IOT Daily Summary - %.1fC avg", 
           (tempMin + tempMax) / 2);
  
  char body[512];
  snprintf(body, sizeof(body),
           "Daily Environmental Summary\n"
           "===========================\n\n"
           "Temperature:\n"
           "  Current: %.1fC\n"
           "  Min: %.1fC\n"
           "  Max: %.1fC\n\n"
           "Humidity:\n"
           "  Current: %.1f%%\n"
           "  Min: %.1f%%\n"
           "  Max: %.1f%%\n\n"
           "Total readings: %d\n"
           "Alert threshold: %.1fC\n",
           temperature, tempMin, tempMax,
           humidity, humMin, humMax,
           readingCount, tempAlertThreshold);
  
  Vwire.email(subject, body);
  Serial.println("Daily summary email sent");
  
  // Reset statistics
  tempMin = 100; tempMax = -100;
  humMin = 100; humMax = 0;
  readingCount = 0;
}

// =============================================================================
// VIRTUAL PIN HANDLERS (Auto-registered via VWIRE_RECEIVE macro)
// =============================================================================

// Threshold slider on V5 - auto-registered, no need for Vwire.onVirtualReceive()
VWIRE_RECEIVE(V5) {
  tempAlertThreshold = param.asFloat();
  
  char msg[64];
  snprintf(msg, sizeof(msg), "Alert threshold set to %.1fC", tempAlertThreshold);
  Vwire.virtualSend(V7, msg);
  Serial.println(msg);
  
  // Re-check alerts with new threshold
  checkAlerts();
}

// =============================================================================
// CONNECTION HANDLERS (Auto-registered via macros)
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Sync threshold setting
  Vwire.virtualSend(V5, tempAlertThreshold);
  Vwire.virtualSend(V6, alertActive ? 1 : 0);
  
  // Log connection
  char msg[64];
  snprintf(msg, sizeof(msg), "Device connected - IP: %s", 
           WiFi.localIP().toString().c_str());
  Vwire.virtualSend(V7, msg);
  
  // Send initial reading
  if (readDHT()) {
    sendSensorData();
  }
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - DHT Sensor Station");
  Serial.println("================================\n");
  
  // Initialize DHT
  dht.begin();
  Serial.println("DHT sensor initialized");
  
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  // ==========================================================================
  // OPTIONAL: Enable Reliable Delivery for critical sensor data
  // ==========================================================================
  // Uncomment these lines if you need guaranteed delivery (e.g., for billing,
  // compliance, or any scenario where losing data points is unacceptable).
  // 
  // Vwire.setReliableDelivery(true);    // Enable ACK-based delivery
  // Vwire.setAckTimeout(5000);          // Wait 5 seconds for server ACK
  // Vwire.setMaxRetries(3);             // Retry up to 3 times
  // 
  // See example 12_ReliableDelivery for full details and delivery callbacks.
  // ==========================================================================
  
  // Note: Handlers are auto-registered via VWIRE_RECEIVE(), VWIRE_CONNECTED(),
  // and VWIRE_DISCONNECTED() macros - no need to call onVirtualReceive() etc.
  
  // Connect
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Initial reading
  readDHT();
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
  
  // Read sensors
  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();
    
    if (readDHT()) {
      checkAlerts();
    }
  }
  
  // Send data to dashboard
  if (Vwire.connected() && millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    sendSensorData();
  }
  
  // Daily summary (email)
  if (millis() - lastDailySummary >= DAILY_INTERVAL) {
    lastDailySummary = millis();
    sendDailySummary();
  }
}
