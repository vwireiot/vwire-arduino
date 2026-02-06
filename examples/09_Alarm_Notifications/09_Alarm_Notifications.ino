/*
 * Vwire IOT - Alarm Notifications Example
 * 
 * This example demonstrates how to send alarms and notifications
 * from your IoT device to the Vwire mobile app.
 * 
 * ⚠️ IMPORTANT: Alarm notifications require a PAID plan (Pro or higher).
 *    Free users can only use notify() for basic push notifications.
 * 
 * Demonstrates:
 * - notify()  - Simple push notification (all users)
 * - alarm()   - Persistent alarm with sound (paid plans only)
 * - email()   - Email notification (paid plans only)
 * 
 * Available Alarm Sounds:
 * - "default" - Standard alarm tone
 * - "urgent"  - High-priority urgent sound
 * - "warning" - Moderate warning sound
 * 
 * Priority Levels:
 * - 1 = Normal (default)
 * - 2 = High
 * - 3 = Critical
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
const char* DEVICE_ID  = "YOUR_DEVICE_ID";

// =============================================================================
// SENSOR PIN (example: smoke detector)
// =============================================================================
const int SENSOR_PIN = 34;  // Analog input pin

// Thresholds for demonstration
const int WARNING_THRESHOLD  = 500;
const int CRITICAL_THRESHOLD = 800;

// Timing
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 5000;  // Check every 5 seconds

// =============================================================================
// HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Send a test notification on connect
  Vwire.notify("Device online and monitoring");
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Vwire IOT Alarm Example ===\n");
  
  // Configure Vwire
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);
  
  // Connect to WiFi and cloud
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
  
  unsigned long now = millis();
  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;
    checkSensorAndAlert();
  }
}

// =============================================================================
// SENSOR CHECK & ALERT LOGIC
// =============================================================================

void checkSensorAndAlert() {
  // Read sensor (simulated for this example)
  int sensorValue = analogRead(SENSOR_PIN);
  
  Serial.printf("Sensor reading: %d\n", sensorValue);
  
  // Send to dashboard
  Vwire.virtualSend(V0, sensorValue);
  
  // Check thresholds and send appropriate alert
  if (sensorValue >= CRITICAL_THRESHOLD) {
    // CRITICAL: Use alarm with urgent sound and highest priority
    Vwire.alarm("CRITICAL: Smoke level dangerous!", "urgent", 3);
    Serial.println(">>> CRITICAL ALARM SENT");
    
  } else if (sensorValue >= WARNING_THRESHOLD) {
    // WARNING: Use alarm with warning sound
    Vwire.alarm("Warning: Elevated smoke level", "warning", 2);
    Serial.println(">>> WARNING ALARM SENT");
  }
  
  // For less urgent updates, use notify()
  // Vwire.notify("Routine check: all systems normal");
}

// =============================================================================
// QUICK REFERENCE
// =============================================================================
/*
 * NOTIFICATIONS (all users):
 *   Vwire.notify("message");
 * 
 * ALARMS (paid plans only):
 *   Vwire.alarm("message");                      // Default sound, normal priority
 *   Vwire.alarm("message", "urgent");            // Urgent sound
 *   Vwire.alarm("message", "warning");           // Warning sound
 *   Vwire.alarm("message", "urgent", 3);         // Urgent + critical priority
 * 
 * Available Sounds: "default", "urgent", "warning"
 * Priority Levels:  1 (normal), 2 (high), 3 (critical)
 * 
 * EMAIL (paid plans only):
 *   Vwire.email("Subject", "Body text");
 * 
 * LOG (debugging):
 *   Vwire.log("Debug message");
 */
