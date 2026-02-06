/*
 * Vwire IOT - Relay Control Example
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: Any WiFi-capable board
 * 
 * Control multiple relays from the Vwire IOT dashboard with:
 * - Individual relay control
 * - All ON/OFF buttons
 * - Scheduled operation (simulated)
 * - Status feedback
 * - Manual override with physical buttons
 * 
 * Dashboard Setup:
 * - V0-V3: Button widgets (Relay 1-4 control)
 * - V4: Button (All ON)
 * - V5: Button (All OFF)
 * - V6: Value display (Active relays count)
 * - V7: LED widgets x4 (Status indicators)
 * 
 * Hardware:
 * - 4-channel relay module on GPIO 16, 17, 18, 19 (ESP32)
 * - Or GPIO 5, 4, 0, 2 (ESP8266)
 * - Physical buttons on GPIO 25, 26, 27, 32 (ESP32)
 * 
 * IMPORTANT: Relays are typically active LOW!
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
// RELAY CONFIGURATION
// =============================================================================
#define NUM_RELAYS 4

#if defined(ESP32)
  const int RELAY_PINS[NUM_RELAYS] = {16, 17, 18, 19};
  const int BUTTON_PINS[NUM_RELAYS] = {25, 26, 27, 32};
#elif defined(ESP8266)
  const int RELAY_PINS[NUM_RELAYS] = {5, 4, 0, 2};
  const int BUTTON_PINS[NUM_RELAYS] = {-1, -1, -1, -1};  // No buttons on ESP8266 example
#else
  const int RELAY_PINS[NUM_RELAYS] = {2, 3, 4, 5};
  const int BUTTON_PINS[NUM_RELAYS] = {6, 7, 8, 9};
#endif

// Relay active state (most relay modules are active LOW)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

const char* RELAY_NAMES[NUM_RELAYS] = {
  "Living Room Light",
  "Bedroom Light", 
  "Kitchen Appliance",
  "Garden Pump"
};

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
bool relayStates[NUM_RELAYS] = {false, false, false, false};
bool lastButtonStates[NUM_RELAYS] = {true, true, true, true};

unsigned long lastDebounce[NUM_RELAYS] = {0};
const unsigned long DEBOUNCE_DELAY = 50;

unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_INTERVAL = 5000;

// =============================================================================
// RELAY CONTROL FUNCTIONS
// =============================================================================

void setRelay(int index, bool state) {
  if (index < 0 || index >= NUM_RELAYS) return;
  
  relayStates[index] = state;
  digitalWrite(RELAY_PINS[index], state ? RELAY_ON : RELAY_OFF);
  
  // Update dashboard
  Vwire.virtualSend(index, state ? 1 : 0);
  
  // Update status LED (V7 + index)
  Vwire.virtualSend(7 + index, state ? 1 : 0);
  
  Serial.printf("Relay %d (%s): %s\n", index + 1, RELAY_NAMES[index], 
                state ? "ON" : "OFF");
}

void setAllRelays(bool state) {
  for (int i = 0; i < NUM_RELAYS; i++) {
    setRelay(i, state);
  }
  
  Vwire.notify(state ? "All relays turned ON" : "All relays turned OFF");
}

int getActiveRelayCount() {
  int count = 0;
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayStates[i]) count++;
  }
  return count;
}

void syncAllRelays() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    Vwire.virtualSend(i, relayStates[i] ? 1 : 0);
    Vwire.virtualSend(V7 + i, relayStates[i] ? 1 : 0);
  }
  Vwire.virtualSend(V6, getActiveRelayCount());
}

// =============================================================================
// PHYSICAL BUTTON HANDLING
// =============================================================================

void checkButtons() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (BUTTON_PINS[i] < 0) continue;  // Skip if no button defined
    
    bool currentState = digitalRead(BUTTON_PINS[i]);
    
    // Debounce
    if (currentState != lastButtonStates[i]) {
      lastDebounce[i] = millis();
    }
    
    if ((millis() - lastDebounce[i]) > DEBOUNCE_DELAY) {
      // Button pressed (active LOW)
      if (currentState == LOW && lastButtonStates[i] == HIGH) {
        // Toggle relay
        setRelay(i, !relayStates[i]);
        
        // Update active count
        Vwire.virtualSend(6, getActiveRelayCount());
      }
    }
    
    lastButtonStates[i] = currentState;
  }
}

// =============================================================================
// VIRTUAL PIN HANDLERS 
// =============================================================================

// Relay control handlers
VWIRE_RECEIVE(V0) { setRelay(0, param.asBool()); Vwire.virtualSend(V6, getActiveRelayCount()); }
VWIRE_RECEIVE(V1) { setRelay(1, param.asBool()); Vwire.virtualSend(V6, getActiveRelayCount()); }
VWIRE_RECEIVE(V2) { setRelay(2, param.asBool()); Vwire.virtualSend(V6, getActiveRelayCount()); }
VWIRE_RECEIVE(V3) { setRelay(3, param.asBool()); Vwire.virtualSend(V6, getActiveRelayCount()); }

// All ON/OFF handlers
VWIRE_RECEIVE(V4) {
  if (param.asInt() == 1) {
    setAllRelays(true);
    Vwire.virtualSend(V6, NUM_RELAYS);
  }
}

VWIRE_RECEIVE(V5) {
  if (param.asInt() == 1) {
    setAllRelays(false);
    Vwire.virtualSend(V6, 0);
  }
}

// =============================================================================
// CONNECTION HANDLERS 
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Request stored relay states from server
  Vwire.sync(V0, V1, V2, V3);
  
  Serial.println("\nRelay Status:");
  for (int i = 0; i < NUM_RELAYS; i++) {
    Serial.printf("  %d. %s: %s\n", i + 1, RELAY_NAMES[i], 
                  relayStates[i] ? "ON" : "OFF");
  }
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
  
  // Optional: Turn off all relays on disconnect for safety
  // setAllRelays(false);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - Relay Control");
  Serial.println("================================\n");
  
  // Initialize relay pins
  Serial.println("Initializing relays...");
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);  // Start with all OFF
    Serial.printf("  Relay %d on GPIO %d\n", i + 1, RELAY_PINS[i]);
  }
  
  // Initialize button pins
  Serial.println("\nInitializing buttons...");
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (BUTTON_PINS[i] >= 0) {
      pinMode(BUTTON_PINS[i], INPUT_PULLUP);
      Serial.printf("  Button %d on GPIO %d\n", i + 1, BUTTON_PINS[i]);
    }
  }
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  // No need to register handlers - VWIRE_RECEIVE() macros auto-register!
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  // Connect
  Serial.println("\nConnecting...");
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
  
  // Check physical buttons
  checkButtons();
  
  // Periodic status update
  if (Vwire.connected() && millis() - lastStatusUpdate >= STATUS_INTERVAL) {
    lastStatusUpdate = millis();
    Vwire.virtualSend(V6, getActiveRelayCount());
  }
}
