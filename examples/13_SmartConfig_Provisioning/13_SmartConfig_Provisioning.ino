/*
 * Vwire IOT - SmartConfig Provisioning Example
 * 
 * This example demonstrates WiFi provisioning using SmartConfig (ESP-Touch).
 * The device receives WiFi credentials and device token from the Vwire mobile app.
 * 
 * How it works:
 * 1. Device starts in SmartConfig listening mode
 * 2. User opens Vwire mobile app → Smart Config
 * 3. User selects a device from their dashboard
 * 4. User enters WiFi credentials
 * 5. App broadcasts encrypted WiFi + token to device
 * 6. Device connects to WiFi and stores credentials
 * 
 * Features:
 * - Automatic credential storage (persists across reboots)
 * - Visual feedback via LED (blinking = waiting, solid = connected)
 * - Falls back to stored credentials on subsequent boots
 * - 2-minute timeout for provisioning
 * 
 * Compatible boards:
 * - ESP32 (all variants)
 * - ESP8266 (NodeMCU, Wemos D1, etc.)
 * 
 * Note: SmartConfig only works with 2.4GHz WiFi networks!
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

// SmartConfig timeout (2 minutes)
#define SMARTCONFIG_TIMEOUT 120000

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
  // Example: LED control from dashboard
  bool value = param.asBool();
  digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  Serial.printf("LED set to: %s\n", value ? "ON" : "OFF");
}

VWIRE_CONNECTED() {
  Serial.println("✓ Connected to Vwire IOT!");
  Serial.printf("  Device: %s\n", Vwire.getDeviceId());
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Solid LED when connected
  digitalWrite(LED_BUILTIN, HIGH);
}

VWIRE_DISCONNECTED() {
  Serial.println("✗ Disconnected from Vwire IOT!");
}

// =============================================================================
// PROVISIONING CALLBACKS
// =============================================================================

void onProvisioningState(VwireProvisioningState state) {
  switch (state) {
    case VWIRE_PROV_SMARTCONFIG_WAIT:
      Serial.println("[Provision] Waiting for SmartConfig...");
      Serial.println("[Provision] Open Vwire app → Smart Config");
      ledBlinkInterval = 200;  // Fast blink = waiting
      break;
      
    case VWIRE_PROV_CONNECTING:
      Serial.println("[Provision] Credentials received! Connecting...");
      ledBlinkInterval = 100;  // Very fast = connecting
      break;
      
    case VWIRE_PROV_SUCCESS:
      Serial.println("[Provision] ✓ Provisioning successful!");
      provisioned = true;
      break;
      
    case VWIRE_PROV_FAILED:
      Serial.println("[Provision] ✗ Provisioning failed!");
      Serial.println("[Provision] Restarting SmartConfig...");
      delay(1000);
      VwireProvision.startSmartConfig(SMARTCONFIG_TIMEOUT);
      break;
      
    case VWIRE_PROV_TIMEOUT:
      Serial.println("[Provision] ✗ SmartConfig timed out!");
      Serial.println("[Provision] Restarting...");
      delay(1000);
      VwireProvision.startSmartConfig(SMARTCONFIG_TIMEOUT);
      break;
      
    default:
      break;
  }
}

void onCredentialsReceived(const char* ssid, const char* password, const char* token) {
  Serial.println("[Provision] Credentials received:");
  Serial.printf("  SSID: %s\n", ssid);
  Serial.println("  Password: [hidden]");
  Serial.printf("  Token: %s\n", token);
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void startProvisioning() {
  Serial.println("\n========================================");
  Serial.println("  Starting SmartConfig Provisioning");
  Serial.println("========================================\n");
  Serial.println("Instructions:");
  Serial.println("1. Open Vwire mobile app");
  Serial.println("2. Go to Settings → Smart Config");
  Serial.println("3. Select your device from the list");
  Serial.println("4. Enter your WiFi credentials");
  Serial.println("5. Tap 'Start Provisioning'\n");
  
  // Set up callbacks
  VwireProvision.setDebug(true);
  VwireProvision.onStateChange(onProvisioningState);
  VwireProvision.onCredentialsReceived(onCredentialsReceived);
  
  // Start SmartConfig
  VwireProvision.startSmartConfig(SMARTCONFIG_TIMEOUT);
}

void connectWithStoredCredentials() {
  Serial.println("\nUsing stored credentials...");
  Serial.printf("  SSID: %s\n", VwireProvision.getSSID());
  Serial.printf("  Token: %s\n", VwireProvision.getAuthToken());
  
  // Configure Vwire with stored token
  Vwire.config(VwireProvision.getAuthToken());
  Vwire.setDebug(true);
  
  // Connect using stored WiFi credentials
  if (Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword())) {
    Serial.println("Connected successfully!");
    provisioned = true;
  } else {
    Serial.println("Failed to connect with stored credentials!");
    Serial.println("Starting SmartConfig for re-provisioning...");
    VwireProvision.clearCredentials();
    startProvisioning();
  }
}

void checkReprovisionButton() {
  // Check if button is pressed (LOW = pressed on most ESP boards)
  if (digitalRead(REPROVISION_BUTTON) == LOW) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
    } else if (millis() - buttonPressStart >= BUTTON_HOLD_TIME) {
      Serial.println("\n*** Re-provisioning triggered by button ***");
      VwireProvision.clearCredentials();
      ESP.restart();
    }
  } else {
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
  Serial.println("  Vwire IOT - SmartConfig Example");
  Serial.println("========================================\n");
  
  // Initialize pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REPROVISION_BUTTON, INPUT_PULLUP);
  
  // Check if we have stored credentials
  if (VwireProvision.hasCredentials()) {
    Serial.println("Found stored credentials!");
    connectWithStoredCredentials();
  } else {
    Serial.println("No stored credentials found.");
    startProvisioning();
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
    
    // WiFi is already connected from provisioning
    if (Vwire.begin()) {
      Serial.println("Connected to Vwire IOT!");
    }
  }
  
  // Normal operation - run Vwire
  Vwire.run();
  
  // Check for re-provisioning button
  checkReprovisionButton();
  
  // Example: Send data periodically
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend >= 10000) {
    lastSend = millis();
    
    // Send some sensor data
    float temperature = 20.0 + random(0, 100) / 10.0;
    Vwire.virtualSend(V1, temperature);
    Serial.printf("Sent temperature: %.1f°C\n", temperature);
  }
}
