/*
 * Vwire IOT - Reliable Delivery Example
 * 
 * Demonstrates the application-level acknowledgment (ACK) system
 * for guaranteed message delivery to the server.
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 * 
 * =============================================================================
 * WHEN TO USE RELIABLE DELIVERY
 * =============================================================================
 * 
 * ✅ USE reliable delivery for:
 *    - Energy/utility metering (every kWh matters for billing)
 *    - Event logging (button presses, door open/close, alerts)
 *    - Infrequent but important readings (daily sensor samples)
 *    - Financial/transactional data
 *    - Any data where loss is unacceptable
 * 
 * ❌ DON'T USE reliable delivery for:
 *    - High-frequency telemetry (100+ messages/minute)
 *    - Real-time streaming where freshness > completeness
 *    - Non-critical status updates
 * 
 * =============================================================================
 * HOW IT WORKS
 * =============================================================================
 * 
 * Standard Mode (fire-and-forget):
 *   Device --[data]--> Server
 *   (No confirmation, fastest, may lose data on network issues)
 * 
 * Reliable Delivery Mode:
 *   Device --[data+msgId]--> Server
 *   Device <--[ACK msgId]--- Server
 *   (Device retries until ACK received or max retries exceeded)
 * 
 * =============================================================================
 * DASHBOARD SETUP
 * =============================================================================
 * 
 * V0: Value Display - Energy reading (kWh)
 * V1: Value Display - Delivery success count
 * V2: Value Display - Delivery failure count
 * V3: LED Widget - Delivery status indicator
 * 
 * =============================================================================
 */

#include <Vwire.h>

// =============================================================================
// CONFIGURATION - UPDATE THESE!
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* AUTH_TOKEN    = "YOUR_AUTH_TOKEN";

// Transport: Use TCP_SSL (port 8883) for TLS encryption (recommended)
//            Use TCP (port 1883) for boards without SSL support
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// RELIABLE DELIVERY SETTINGS
// =============================================================================
// These are the default values - customize as needed:

const bool RELIABLE_ENABLED = true;      // Enable reliable delivery
const unsigned long ACK_TIMEOUT = 5000;  // Wait 5 seconds for ACK
const uint8_t MAX_RETRIES = 3;           // Retry up to 3 times

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Simulated energy meter (replace with real sensor reading)
float energyReading = 0.0;

// Delivery statistics
unsigned long successCount = 0;
unsigned long failureCount = 0;

// Timing
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 60000;  // Send every 60 seconds (1 minute)

// =============================================================================
// DELIVERY STATUS CALLBACK
// =============================================================================
// This function is called when the server confirms (or fails) message delivery.
// Use this to:
//   - Track delivery statistics
//   - Store failed messages to flash for later retry
//   - Alert user of persistent failures
//   - Update UI indicators

void onDeliveryResult(const char* msgId, bool success) {
  if (success) {
    // ✓ Message was received and stored by server
    successCount++;
    Serial.printf("✓ Delivered [%s] - Total: %lu successful\n", msgId, successCount);
    
    // Update dashboard LED (green = success)
    Vwire.virtualSend(V3, 1);
    
  } else {
    // ✗ Message failed after all retries
    failureCount++;
    Serial.printf("✗ FAILED [%s] - Total: %lu failures\n", msgId, failureCount);
    
    // Update dashboard LED (red = failure)
    Vwire.virtualSend(V3, 0);
    
    // IMPORTANT: For critical data, you might want to:
    // 1. Store to SPIFFS/LittleFS for later retry
    // 2. Send an alert notification
    // 3. Trigger a local backup mechanism
    
    // Example: Send failure notification
    // Vwire.notify("Data delivery failed! Check device connection.");
  }
  
  // Update statistics on dashboard
  Vwire.virtualSend(V1, (int)successCount);
  Vwire.virtualSend(V2, (int)failureCount);
}

// =============================================================================
// SIMULATED SENSOR READING
// =============================================================================
// Replace this with your actual sensor reading code

float readEnergySensor() {
  // Simulate energy consumption (increments over time)
  // In real use: read from CT sensor, pulse counter, or energy IC
  energyReading += random(1, 10) / 100.0;  // Add 0.01-0.09 kWh
  return energyReading;
}

// =============================================================================
// CONNECTION HANDLER
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  Serial.printf("  Reliable Delivery: %s\n", RELIABLE_ENABLED ? "ENABLED" : "DISABLED");
  Serial.printf("  ACK Timeout: %lu ms\n", ACK_TIMEOUT);
  Serial.printf("  Max Retries: %d\n", MAX_RETRIES);
  
  // Sync any stored values from server
  Vwire.sync(V0, V1, V2, V3);
  
  // Print debug info
  Vwire.printDebugInfo();
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected from server!");
  Serial.printf("  Pending messages: %d\n", Vwire.getPendingCount());
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("  Vwire IOT - Reliable Delivery Demo");
  Serial.println("========================================\n");
  
  // -----------------------------------------
  // Step 1: Configure Vwire connection
  // -----------------------------------------
  Vwire.config(AUTH_TOKEN);
  Vwire.setTransport(TRANSPORT);
  Vwire.setDebug(true);  // Enable debug output
  
  // -----------------------------------------
  // Step 2: Configure Reliable Delivery
  // -----------------------------------------
  // IMPORTANT: These must be set BEFORE calling begin()
  
  Vwire.setReliableDelivery(RELIABLE_ENABLED);  // Enable ACK-based delivery
  Vwire.setAckTimeout(ACK_TIMEOUT);             // Time to wait for server ACK
  Vwire.setMaxRetries(MAX_RETRIES);             // Retry attempts before giving up
  Vwire.onDeliveryStatus(onDeliveryResult);     // Register callback for results
  
  // -----------------------------------------
  // Step 3: Connect
  // -----------------------------------------
  Serial.println("Connecting to WiFi and MQTT...");
  
  if (Vwire.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("✓ Connected successfully!");
  } else {
    Serial.println("✗ Connection failed!");
    Serial.println("  Check credentials and try again.");
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // CRITICAL: Must call run() frequently!
  // This processes incoming ACKs and handles retries
  Vwire.run();
  
  // -----------------------------------------
  // Send energy reading at regular intervals
  // -----------------------------------------
  if (Vwire.connected() && millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    
    // Read the sensor
    float kWh = readEnergySensor();
    
    // Send to server with reliable delivery
    // The virtualSend() automatically uses reliable delivery when enabled
    Vwire.virtualSend(V0, kWh);
    
    Serial.printf("\n📊 Sent: %.2f kWh\n", kWh);
    Serial.printf("   Pending ACKs: %d\n", Vwire.getPendingCount());
    Serial.printf("   Free Heap: %u bytes\n", Vwire.getFreeHeap());
  }
  
  // -----------------------------------------
  // Optional: Check pending message status
  // -----------------------------------------
  // You can monitor the delivery queue at any time:
  
  static unsigned long lastStatusCheck = 0;
  if (millis() - lastStatusCheck >= 10000) {  // Every 10 seconds
    lastStatusCheck = millis();
    
    if (Vwire.isDeliveryPending()) {
      Serial.printf("⏳ Waiting for %d ACK(s)...\n", Vwire.getPendingCount());
    }
  }
}

// =============================================================================
// ADVANCED: STORING FAILED MESSAGES TO FLASH
// =============================================================================
// 
// For truly critical data, you may want to store failed messages locally
// and retry them later. Here's a conceptual example:
//
// #include <LittleFS.h>
//
// void storeFailedMessage(float value) {
//   File f = LittleFS.open("/failed.log", "a");
//   if (f) {
//     f.printf("%lu,%.2f\n", millis(), value);
//     f.close();
//   }
// }
//
// void retryStoredMessages() {
//   File f = LittleFS.open("/failed.log", "r");
//   if (f && f.size() > 0) {
//     // Read and resend stored messages
//     // Delete file after successful delivery
//   }
// }
//
// =============================================================================
