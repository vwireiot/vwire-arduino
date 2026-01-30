/*
 * Vwire IOT - AP Mode Provisioning Example
 * 
 * This example demonstrates WiFi provisioning using Access Point (AP) mode.
 * The device creates a WiFi hotspot, and users connect to it to configure
 * WiFi credentials and device token through a web interface.
 * 
 * How it works:
 * 1. Device creates a WiFi hotspot (e.g., "Vwire-XXXX")
 * 2. User connects their phone/computer to the hotspot
 * 3. User opens browser to http://192.168.4.1
 * 4. User enters WiFi credentials and device token
 * 5. Device saves credentials and connects to WiFi
 * 
 * Features:
 * - Works when SmartConfig fails (5GHz networks, etc.)
 * - Beautiful responsive web interface
 * - Network scanning for available WiFi networks
 * - Automatic credential storage (persists across reboots)
 * - Captive portal support (auto-opens configuration page)
 * 
 * Compatible boards:
 * - ESP32 (all variants)
 * - ESP8266 (NodeMCU, Wemos D1, etc.)
 * 
 * Note: The AP will be named "Vwire-XXXX" where XXXX is 
 * the last 4 characters of the device MAC address.
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>
#include <VwireProvisioning.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

// LED for visual feedback
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// Button to trigger re-provisioning (optional)
#define REPROVISION_BUTTON 0  // GPIO0 = Flash button on most ESP boards
#define BUTTON_HOLD_TIME 5000  // Hold for 5 seconds to reset

// AP Mode settings
#define AP_PASSWORD "vwire123"  // Set to nullptr for open network
#define AP_TIMEOUT 300000       // 5 minutes timeout

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
unsigned long ledBlinkInterval = 500;
unsigned long lastLedBlink = 0;
bool ledState = false;
bool provisioned = false;
unsigned long buttonPressStart = 0;

// =============================================================================
// VWIRE HANDLERS
// =============================================================================

VWIRE_RECEIVE(V0) {
  bool value = param.asBool();
  digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  Serial.printf("LED set to: %s\n", value ? "ON" : "OFF");
}

VWIRE_RECEIVE(V2) {
  // Control an output pin
  int pinNumber = 2;
  bool state = param.asBool();
  digitalWrite(pinNumber, state);
  Serial.printf("Pin %d set to: %s\n", pinNumber, state ? "HIGH" : "LOW");
}

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  Serial.printf("  Device Token: %s\n", VwireProvision.getAuthToken());
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Solid LED when connected
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Sync current state
  Vwire.virtualSync(V0);
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected from Vwire IOT!");
}

// =============================================================================
// PROVISIONING CALLBACKS
// =============================================================================

void onProvisioningState(VwireProvisioningState state) {
  switch (state) {
    case VWIRE_PROV_AP_ACTIVE:
      Serial.println("\n========================================");
      Serial.println("  AP Mode Active - Configuration Portal");
      Serial.println("========================================\n");
      Serial.printf("1. Connect to WiFi: %s\n", VwireProvision.getAPSSID());
      if (AP_PASSWORD) {
        Serial.printf("   Password: %s\n", AP_PASSWORD);
      } else {
        Serial.println("   (Open network - no password)");
      }
      Serial.printf("2. Open browser: http://%s\n", VwireProvision.getAPIP().toString().c_str());
      Serial.println("3. Enter your WiFi credentials and device token\n");
      ledBlinkInterval = 1000;  // Slow blink = AP active
      break;
      
    case VWIRE_PROV_CONNECTING:
      Serial.println("[Provision] Connecting to WiFi...");
      ledBlinkInterval = 100;
      break;
      
    case VWIRE_PROV_SUCCESS:
      Serial.println("[Provision] ✓ Provisioning successful!");
      Serial.printf("[Provision] Connected to: %s\n", VwireProvision.getSSID());
      provisioned = true;
      break;
      
    case VWIRE_PROV_FAILED:
      Serial.println("[Provision] ✗ Connection failed!");
      Serial.println("[Provision] Check your credentials and try again.");
      // AP mode stays active, user can retry
      ledBlinkInterval = 1000;
      break;
      
    case VWIRE_PROV_TIMEOUT:
      Serial.println("[Provision] AP Mode timed out!");
      Serial.println("[Provision] Restarting AP...");
      delay(2000);
      VwireProvision.startAPMode(AP_PASSWORD, AP_TIMEOUT);
      break;
      
    default:
      break;
  }
}

void onCredentialsReceived(const char* ssid, const char* password, const char* token) {
  Serial.println("[Provision] Configuration received from web interface:");
  Serial.printf("  SSID: %s\n", ssid);
  Serial.println("  Password: [hidden]");
  Serial.printf("  Token: %s\n", token);
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void startAPProvisioning() {
  Serial.println("\nStarting AP Mode Provisioning...\n");
  
  // Set up callbacks
  VwireProvision.setDebug(true);
  VwireProvision.onStateChange(onProvisioningState);
  VwireProvision.onCredentialsReceived(onCredentialsReceived);
  
  // Start AP mode with password and timeout
  VwireProvision.startAPMode(AP_PASSWORD, AP_TIMEOUT);
}

void connectWithStoredCredentials() {
  Serial.println("\nUsing stored credentials...");
  Serial.printf("  SSID: %s\n", VwireProvision.getSSID());
  Serial.printf("  Token: %s\n", VwireProvision.getAuthToken());
  
  // Configure Vwire with stored token
  Vwire.config(VwireProvision.getAuthToken());
  Vwire.setDebug(true);
  
  // Connect using stored WiFi credentials
  Serial.println("Connecting to WiFi...");
  if (Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword())) {
    Serial.println("Connected successfully!");
    provisioned = true;
  } else {
    Serial.println("Failed to connect with stored credentials!");
    Serial.println("Starting AP Mode for re-provisioning...");
    VwireProvision.clearCredentials();
    startAPProvisioning();
  }
}

void checkReprovisionButton() {
  if (digitalRead(REPROVISION_BUTTON) == LOW) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
      Serial.println("Button pressed - hold for 5 seconds to reset credentials");
    } else if (millis() - buttonPressStart >= BUTTON_HOLD_TIME) {
      Serial.println("\n*** Re-provisioning triggered by button ***");
      Serial.println("Clearing stored credentials...");
      VwireProvision.clearCredentials();
      delay(500);
      ESP.restart();
    }
  } else {
    if (buttonPressStart > 0 && millis() - buttonPressStart < BUTTON_HOLD_TIME) {
      Serial.println("Button released - hold longer to reset");
    }
    buttonPressStart = 0;
  }
}

void blinkLED() {
  if (millis() - lastLedBlink >= ledBlinkInterval) {
    lastLedBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("  Vwire IOT - AP Mode Provisioning");
  Serial.println("========================================\n");
  
  // Initialize pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REPROVISION_BUTTON, INPUT_PULLUP);
  
  // Print MAC address for identification
  Serial.printf("Device MAC: %s\n", WiFi.macAddress().c_str());
  
  // Check if we have stored credentials
  if (VwireProvision.hasCredentials()) {
    Serial.println("Found stored credentials!");
    connectWithStoredCredentials();
  } else {
    Serial.println("No stored credentials found.");
    startAPProvisioning();
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // Process provisioning if active
  if (VwireProvision.isProvisioning()) {
    VwireProvision.run();
    blinkLED();
    return;
  }
  
  // After provisioning success, connect to Vwire
  if (provisioned && !Vwire.connected() && VwireProvision.getState() == VWIRE_PROV_SUCCESS) {
    Serial.println("\nConnecting to Vwire IOT...");
    Vwire.config(VwireProvision.getAuthToken());
    Vwire.setDebug(true);
    
    if (Vwire.begin()) {
      Serial.println("Connected to Vwire IOT!");
    }
  }
  
  // Normal operation
  Vwire.run();
  
  // Check for re-provisioning button
  checkReprovisionButton();
  
  // Example: Send data every 30 seconds
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend >= 30000) {
    lastSend = millis();
    
    // Send uptime in seconds
    unsigned long uptime = millis() / 1000;
    Vwire.virtualSend(V3, (int)uptime);
    Serial.printf("Sent uptime: %lu seconds\n", uptime);
    
    // Send free heap memory
    uint32_t freeHeap = ESP.getFreeHeap();
    Vwire.virtualSend(V4, (int)freeHeap);
    Serial.printf("Free heap: %u bytes\n", freeHeap);
  }
}
