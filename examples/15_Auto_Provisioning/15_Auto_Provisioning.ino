/*
 * Vwire IOT - Auto Provisioning Example (Recommended)
 * 
 * This is the RECOMMENDED provisioning example that combines both SmartConfig
 * and AP Mode for the best user experience. It automatically handles:
 * 
 * 1. First boot: Starts SmartConfig for easy mobile app provisioning
 * 2. SmartConfig timeout: Falls back to AP Mode as backup
 * 3. Subsequent boots: Uses stored credentials
 * 4. Connection failure: Re-enters provisioning mode
 * 
 * Why use this approach?
 * - SmartConfig is faster and more convenient (no need to change WiFi)
 * - AP Mode provides a fallback for networks that don't support SmartConfig
 * - Credentials are stored and persist across reboots
 * - Button allows users to reset and re-provision anytime
 * 
 * Provisioning Flow:
 * ┌─────────────┐     ┌──────────────┐     ┌─────────────┐
 * │  Power On   │────>│ Check Stored │────>│  Connect    │
 * └─────────────┘     │ Credentials  │     │  to WiFi    │
 *                     └──────────────┘     └─────────────┘
 *                            │                    │
 *                         No │                    │ Success
 *                            ▼                    ▼
 *                     ┌──────────────┐     ┌─────────────┐
 *                     │ SmartConfig  │     │  Connect    │
 *                     │   (60 sec)   │     │  to Vwire   │
 *                     └──────────────┘     └─────────────┘
 *                            │
 *                      Timeout
 *                            ▼
 *                     ┌──────────────┐
 *                     │  AP Mode     │
 *                     │  (fallback)  │
 *                     └──────────────┘
 * 
 * Compatible boards:
 * - ESP32 (all variants)
 * - ESP8266 (NodeMCU, Wemos D1, etc.)
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

// Button to trigger re-provisioning
#define REPROVISION_BUTTON 0    // GPIO0 = Flash button on most ESP boards
#define BUTTON_HOLD_TIME 3000   // Hold for 3 seconds to reset

// Provisioning timeouts
#define SMARTCONFIG_TIMEOUT 60000   // 1 minute for SmartConfig
#define AP_MODE_TIMEOUT 0           // No timeout for AP mode (stays until configured)

// AP Mode settings
#define AP_PASSWORD "vwire123"      // Set to nullptr for open network

// =============================================================================
// LED BLINK PATTERNS
// =============================================================================
#define BLINK_SMARTCONFIG 200   // Fast blink - SmartConfig active
#define BLINK_AP_MODE 1000      // Slow blink - AP Mode active
#define BLINK_CONNECTING 50     // Very fast - Connecting
#define BLINK_ERROR 100         // Rapid blink - Error

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
unsigned long ledBlinkInterval = 500;
unsigned long lastLedBlink = 0;
bool ledState = false;
bool provisioned = false;
unsigned long buttonPressStart = 0;
bool smartConfigTried = false;

// =============================================================================
// VWIRE HANDLERS (Customize for your application)
// =============================================================================

// Virtual Pin 0: LED control
VWIRE_RECEIVE(V0) {
  bool value = param.asBool();
  digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  Serial.printf("[App] LED set to: %s\n", value ? "ON" : "OFF");
}

// Virtual Pin 1: Receive a value
VWIRE_RECEIVE(V1) {
  int value = param.asInt();
  Serial.printf("[App] V1 received: %d\n", value);
}

VWIRE_CONNECTED() {
  Serial.println("\n✓ Connected to Vwire IOT!");
  Serial.println("========================================");
  Serial.printf("  Device: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  WiFi: %s\n", WiFi.SSID().c_str());
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
  Serial.println("========================================\n");
  
  // Solid LED when connected
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Sync virtual pins to get current state from server
  Vwire.syncVirtual(V0);
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
      Serial.println("\n[Provision] SmartConfig listening...");
      Serial.println("[Provision] Open Vwire app → Smart Config");
      Serial.println("[Provision] (Will switch to AP Mode in 60 seconds)\n");
      ledBlinkInterval = BLINK_SMARTCONFIG;
      break;
      
    case VWIRE_PROV_AP_ACTIVE:
      Serial.println("\n========================================");
      Serial.println("  AP Mode - Manual Configuration");
      Serial.println("========================================");
      Serial.printf("1. Connect to WiFi: %s\n", VwireProvision.getAPSSID());
      if (AP_PASSWORD) {
        Serial.printf("   Password: %s\n", AP_PASSWORD);
      }
      Serial.printf("2. Open: http://%s\n", VwireProvision.getAPIP().c_str());
      Serial.println("========================================\n");
      ledBlinkInterval = BLINK_AP_MODE;
      break;
      
    case VWIRE_PROV_CONNECTING:
      Serial.println("[Provision] Connecting to WiFi...");
      ledBlinkInterval = BLINK_CONNECTING;
      break;
      
    case VWIRE_PROV_SUCCESS:
      Serial.println("[Provision] ✓ Provisioning complete!");
      provisioned = true;
      break;
      
    case VWIRE_PROV_FAILED:
      Serial.println("[Provision] ✗ Connection failed!");
      ledBlinkInterval = BLINK_ERROR;
      // If in AP mode, it stays active for retry
      // If SmartConfig failed, it will timeout and switch to AP mode
      break;
      
    case VWIRE_PROV_TIMEOUT:
      if (!smartConfigTried) {
        Serial.println("\n[Provision] SmartConfig timed out.");
        Serial.println("[Provision] Switching to AP Mode...\n");
        smartConfigTried = true;
        VwireProvision.startAPMode(AP_PASSWORD, AP_MODE_TIMEOUT);
      }
      break;
      
    default:
      break;
  }
}

void onCredentialsReceived(const char* ssid, const char* password, const char* token) {
  Serial.println("[Provision] Credentials received:");
  Serial.printf("  SSID: %s\n", ssid);
  Serial.printf("  Token: %s...\n", String(token).substring(0, 8).c_str());
}

void onProgress(int percent, const char* message) {
  Serial.printf("[Provision] %d%% - %s\n", percent, message);
}

// =============================================================================
// PROVISIONING FUNCTIONS
// =============================================================================

void startAutoProvisioning() {
  Serial.println("\n========================================");
  Serial.println("  Auto Provisioning Started");
  Serial.println("========================================");
  Serial.println("Mode 1: SmartConfig (easiest - use Vwire app)");
  Serial.println("Mode 2: AP Mode (fallback after 60 seconds)\n");
  
  // Set up callbacks
  VwireProvision.setDebug(true);
  VwireProvision.onStateChange(onProvisioningState);
  VwireProvision.onCredentialsReceived(onCredentialsReceived);
  VwireProvision.onProgress(onProgress);
  
  // Start with SmartConfig
  smartConfigTried = false;
  VwireProvision.startSmartConfig(SMARTCONFIG_TIMEOUT);
}

void connectWithStoredCredentials() {
  Serial.println("\nUsing stored credentials...");
  Serial.printf("  SSID: %s\n", VwireProvision.getSSID());
  
  // Configure Vwire with stored token
  Vwire.config(VwireProvision.getAuthToken());
  Vwire.setDebug(true);
  
  // Set connection timeout
  Vwire.setConnectTimeout(30000);
  
  // Connect using stored WiFi credentials
  Serial.println("Connecting to WiFi...");
  if (Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword())) {
    Serial.println("WiFi connected!");
    provisioned = true;
  } else {
    Serial.println("\n✗ Failed to connect with stored credentials!");
    Serial.println("This can happen if:");
    Serial.println("  - WiFi network is unavailable");
    Serial.println("  - Password was changed");
    Serial.println("  - Router is unreachable");
    Serial.println("\nStarting re-provisioning...\n");
    VwireProvision.clearCredentials();
    startAutoProvisioning();
  }
}

void resetToFactoryDefaults() {
  Serial.println("\n========================================");
  Serial.println("  Factory Reset Triggered");
  Serial.println("========================================");
  Serial.println("Clearing all stored credentials...");
  
  VwireProvision.clearCredentials();
  
  Serial.println("Restarting device...\n");
  delay(1000);
  ESP.restart();
}

// =============================================================================
// BUTTON HANDLING
// =============================================================================

void checkReprovisionButton() {
  if (digitalRead(REPROVISION_BUTTON) == LOW) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
    } else {
      unsigned long held = millis() - buttonPressStart;
      
      // Visual feedback while holding
      if (held > 1000 && held < BUTTON_HOLD_TIME) {
        // Faster blink as approaching reset
        ledBlinkInterval = map(held, 1000, BUTTON_HOLD_TIME, 200, 50);
      }
      
      if (held >= BUTTON_HOLD_TIME) {
        resetToFactoryDefaults();
      }
    }
  } else {
    if (buttonPressStart > 0) {
      buttonPressStart = 0;
      // Restore normal blink rate
      if (provisioned) {
        digitalWrite(LED_BUILTIN, HIGH);
      }
    }
  }
}

// =============================================================================
// LED CONTROL
// =============================================================================

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
  
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║     Vwire IOT - Auto Provisioning      ║");
  Serial.println("║           Production Ready             ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Initialize pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REPROVISION_BUTTON, INPUT_PULLUP);
  
  // Show device info
  Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Flash Size: %u bytes\n", ESP.getFlashChipSize());
  
  #ifdef ESP32
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  #endif
  
  // Check for stored credentials
  if (VwireProvision.hasCredentials()) {
    Serial.println("\n✓ Found stored credentials");
    connectWithStoredCredentials();
  } else {
    Serial.println("\n○ No stored credentials");
    Serial.println("  Hold FLASH button for 3 seconds to reset anytime");
    startAutoProvisioning();
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // Check button first (always active)
  checkReprovisionButton();
  
  // Process provisioning if active
  if (VwireProvision.isProvisioning()) {
    VwireProvision.run();
    blinkLED();
    return;
  }
  
  // After provisioning success, connect to Vwire
  if (provisioned && !Vwire.connected() && VwireProvision.getState() == VWIRE_PROV_SUCCESS) {
    Serial.println("\nProvisioning complete. Connecting to Vwire IOT...");
    Vwire.config(VwireProvision.getAuthToken());
    Vwire.setDebug(true);
    
    if (Vwire.begin()) {
      Serial.println("Successfully connected to Vwire IOT!");
    } else {
      Serial.println("Failed to connect to Vwire. Retrying...");
    }
  }
  
  // Normal operation - run Vwire
  Vwire.run();
  
  // ==========================================================================
  // YOUR APPLICATION CODE GOES HERE
  // ==========================================================================
  
  // Example: Send sensor data every 10 seconds
  static unsigned long lastSensorRead = 0;
  if (Vwire.connected() && millis() - lastSensorRead >= 10000) {
    lastSensorRead = millis();
    
    // Send some example data
    float temperature = 22.5 + (random(-20, 20) / 10.0);
    float humidity = 55.0 + (random(-100, 100) / 10.0);
    int rssi = WiFi.RSSI();
    
    // Send to virtual pins
    Vwire.virtualSend(V2, temperature);
    Vwire.virtualSend(V3, humidity);
    Vwire.virtualSend(V4, rssi);
    
    Serial.printf("[Sensor] Temp: %.1f°C, Humidity: %.1f%%, RSSI: %d dBm\n", 
                  temperature, humidity, rssi);
  }
  
  // Example: Handle local button press
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(REPROVISION_BUTTON);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    // Button just pressed (short press, not held)
    if (buttonPressStart == 0) {
      Serial.println("[Button] Short press detected");
      // Toggle LED and sync to server
      ledState = !ledState;
      Vwire.virtualSend(V0, ledState);
    }
  }
  lastButtonState = currentButtonState;
}
