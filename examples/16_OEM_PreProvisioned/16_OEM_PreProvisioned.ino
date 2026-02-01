/*
 * Vwire IOT - OEM Pre-Provisioned Device Example
 * 
 * This example is for OEM (Original Equipment Manufacturer) devices that are
 * pre-provisioned in the Vwire admin panel before being sold to customers.
 * 
 * OEM Workflow:
 * 1. OEM admin creates devices in bulk via Vwire Admin Panel
 * 2. Each device gets a unique Device ID and Auth Token
 * 3. OEM flashes firmware with pre-configured AUTH TOKEN
 * 4. Device is ready-to-connect when customer receives it
 * 5. Customer only needs to provide WiFi credentials (via AP Mode)
 * 6. When customer claims device, a template project is auto-created
 * 
 * Admin Panel Bulk Provisioning:
 *   - Go to Admin Panel > Devices > Provision Devices
 *   - Select "Bulk Create" and specify:
 *     - Count: Number of devices to create
 *     - Device Type: SWITCH, DIMMER, FAN_CONTROLLER, etc.
 *     - Hardware Type: ESP32, ESP8266, etc.
 *     - Batch ID: Optional identifier for this production batch
 *     - Expiry Date: Optional device expiration date
 *   - Download credentials CSV for firmware flashing
 * 
 * Key Differences from Consumer Provisioning:
 *   - Auth Token is pre-configured at compile time
 *   - User only provides WiFi credentials during setup
 *   - Device type determines the template project created on claim
 *   - OEM can track devices by batch ID and serial number
 * 
 * IMPORTANT: 
 *   - The AUTH TOKEN is the secret used for MQTT authentication
 *   - The Device ID is only for display/identification
 *   - Flash the AUTH TOKEN from the CSV into the firmware
 * 
 * Compatible boards:
 *   - ESP32 (all variants)
 *   - ESP8266 (NodeMCU, Wemos D1, etc.)
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>
#include <VwireProvisioning.h>

// =============================================================================
// OEM PRE-PROVISIONED CREDENTIALS
// Get these from Vwire Admin Panel bulk provisioning CSV
// =============================================================================

// IMPORTANT: Replace this with the AUTH TOKEN from Admin Panel CSV
// The auth token is the secret key used for device authentication
#define VWIRE_AUTH_TOKEN  "your-auth-token-from-admin-panel"

// Device ID for display purposes (shown on QR code sticker, etc.)
// This helps users identify their device during claiming
#define VWIRE_DEVICE_ID_DISPLAY   "VW-ABC123"

// Device metadata (optional, helps with tracking)
#define VWIRE_BATCH_ID    "BATCH-2025-001"
#define VWIRE_SERIAL_NUM  "SN-00001"

// =============================================================================
// DEVICE CONFIGURATION
// This should match the device type set in Admin Panel
// =============================================================================

// Device type: Determines the template project created when user claims
// Options: SWITCH, SWITCH_2CH, SWITCH_4CH, DIMMER, FAN_CONTROLLER, 
//          RGB_LED, SENSOR_TEMP, SMART_PLUG, GENERIC
#define DEVICE_TYPE "SWITCH"

// Number of relay outputs (for switch-type devices)
#define NUM_RELAYS 1

// Relay GPIO pins
const uint8_t RELAY_PINS[] = { 12 };  // GPIO12 for single relay

// Button for manual control (optional)
#define MANUAL_BUTTON_PIN 0  // GPIO0 (Flash button)

// LED indicator
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// =============================================================================
// AP MODE SETTINGS (for WiFi provisioning only)
// =============================================================================

// AP Mode will use device ID as SSID suffix: "VWire_VW-ABC123"
// Password for AP (set to nullptr for open network)
#define AP_PASSWORD "vwire123"

// Button hold time to trigger WiFi reset (milliseconds)
#define RESET_BUTTON_HOLD_TIME 5000

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

bool relayStates[NUM_RELAYS] = { false };
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool wifiProvisioned = false;

// =============================================================================
// VWIRE HANDLERS - These match the template widgets
// =============================================================================

// Virtual Pin 0: Relay 1 Control
VWIRE_RECEIVE(V0) {
  bool value = param.asBool();
  relayStates[0] = value;
  digitalWrite(RELAY_PINS[0], value ? HIGH : LOW);
  
  // Sync state back to dashboard
  Vwire.virtualSend(V0, value);
  
  Serial.printf("[OEM] Relay 1 set to: %s\n", value ? "ON" : "OFF");
}

// Add more handlers for additional relays if NUM_RELAYS > 1
#if NUM_RELAYS >= 2
VWIRE_RECEIVE(V1) {
  bool value = param.asBool();
  relayStates[1] = value;
  digitalWrite(RELAY_PINS[1], value ? HIGH : LOW);
  Vwire.virtualSend(V1, value);
  Serial.printf("[OEM] Relay 2 set to: %s\n", value ? "ON" : "OFF");
}
#endif

#if NUM_RELAYS >= 4
VWIRE_RECEIVE(V2) {
  bool value = param.asBool();
  relayStates[2] = value;
  digitalWrite(RELAY_PINS[2], value ? HIGH : LOW);
  Vwire.virtualSend(V2, value);
  Serial.printf("[OEM] Relay 3 set to: %s\n", value ? "ON" : "OFF");
}

VWIRE_RECEIVE(V3) {
  bool value = param.asBool();
  relayStates[3] = value;
  digitalWrite(RELAY_PINS[3], value ? HIGH : LOW);
  Vwire.virtualSend(V3, value);
  Serial.printf("[OEM] Relay 4 set to: %s\n", value ? "ON" : "OFF");
}
#endif

// =============================================================================
// CONNECTION EVENT HANDLERS
// =============================================================================

void onVwireConnected() {
  Serial.println("[OEM] Connected to Vwire Cloud!");
  digitalWrite(LED_BUILTIN, HIGH);  // Solid LED = connected
  
  // Sync current states to dashboard on connect
  for (int i = 0; i < NUM_RELAYS; i++) {
    Vwire.virtualSend(i, relayStates[i]);
  }
  
  // Report device metadata
  Serial.printf("[OEM] Device ID (display): %s\n", VWIRE_DEVICE_ID_DISPLAY);
  Serial.printf("[OEM] Batch: %s\n", VWIRE_BATCH_ID);
  Serial.printf("[OEM] Serial: %s\n", VWIRE_SERIAL_NUM);
}

void onVwireDisconnected() {
  Serial.println("[OEM] Disconnected from Vwire Cloud");
  // Blink LED to show disconnected state
}

// =============================================================================
// WIFI PROVISIONING HANDLERS
// =============================================================================

void onProvisioningStateChanged(VwireProvisioningState state) {
  switch (state) {
    case VWIRE_PROV_AP_ACTIVE:
      Serial.println("[OEM] AP Mode active - Connect to: " + String(VwireProvision.getAPSSID()));
      Serial.println("[OEM] Configure at: http://" + VwireProvision.getAPIP());
      break;
    case VWIRE_PROV_CONNECTING:
      Serial.println("[OEM] Connecting to WiFi...");
      break;
    case VWIRE_PROV_SUCCESS:
      Serial.println("[OEM] WiFi provisioning successful!");
      wifiProvisioned = true;
      break;
    case VWIRE_PROV_FAILED:
      Serial.println("[OEM] WiFi provisioning failed");
      break;
    default:
      break;
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║     Vwire OEM Pre-Provisioned Device      ║");
  Serial.println("╠═══════════════════════════════════════════╣");
  Serial.printf ("║ Device ID: %-30s ║\n", VWIRE_DEVICE_ID_DISPLAY);
  Serial.printf ("║ Batch ID:  %-30s ║\n", VWIRE_BATCH_ID);
  Serial.printf ("║ Serial:    %-30s ║\n", VWIRE_SERIAL_NUM);
  Serial.printf ("║ Type:      %-30s ║\n", DEVICE_TYPE);
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println();

  // Initialize GPIO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  pinMode(MANUAL_BUTTON_PIN, INPUT_PULLUP);

  // Configure Vwire with pre-provisioned AUTH TOKEN
  // The auth token is the secret key used for MQTT authentication
  Vwire.config(VWIRE_AUTH_TOKEN);
  
  // Set connection handlers
  Vwire.onConnect(onVwireConnected);
  Vwire.onDisconnect(onVwireDisconnected);

  // Set provisioning callbacks
  VwireProvision.onStateChange(onProvisioningStateChanged);
  VwireProvision.setDebug(true);

  // Check if WiFi credentials are stored
  if (VwireProvision.hasCredentials()) {
    Serial.println("[OEM] WiFi credentials found, connecting...");
    
    // Use stored WiFi credentials
    Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword());
    wifiProvisioned = true;
  } else {
    Serial.println("[OEM] No WiFi credentials, starting AP Mode...");
    Serial.println("[OEM] NOTE: Device ID and Auth Token are pre-configured");
    Serial.println("[OEM]       Only WiFi credentials needed from user");
    
    // Start AP Mode with device ID as part of SSID (for user identification)
    // Note: The device ID is just for display - auth token is what matters
    // OEM Mode = true: Token field will NOT be shown in portal (WiFi only)
    String apSSID = "VWire_" + String(VWIRE_DEVICE_ID_DISPLAY);
    VwireProvision.startAPMode(apSSID.c_str(), AP_PASSWORD, 0, true);  // true = OEM mode
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  // Run Vwire (handles MQTT, commands, etc.)
  Vwire.run();
  
  // Run provisioning (handles AP Mode web server)
  VwireProvision.run();
  
  // Check if provisioning just completed
  if (VwireProvision.getState() == VWIRE_PROV_SUCCESS && !Vwire.connected()) {
    Serial.println("[OEM] WiFi configured, restarting to connect...");
    delay(1000);
    ESP.restart();
  }
  
  // Handle manual button press
  handleManualButton();
  
  // Update LED indicator
  updateLED();
  
  delay(10);
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void handleManualButton() {
  bool currentState = (digitalRead(MANUAL_BUTTON_PIN) == LOW);
  
  if (currentState && !buttonPressed) {
    // Button just pressed
    buttonPressStart = millis();
    buttonPressed = true;
  } else if (!currentState && buttonPressed) {
    // Button released
    unsigned long pressDuration = millis() - buttonPressStart;
    
    if (pressDuration >= RESET_BUTTON_HOLD_TIME) {
      // Long press: Reset WiFi credentials
      Serial.println("[OEM] Resetting WiFi credentials...");
      VwireProvision.clearCredentials();
      delay(500);
      ESP.restart();
    } else if (pressDuration >= 50) {
      // Short press: Toggle relay 1
      relayStates[0] = !relayStates[0];
      digitalWrite(RELAY_PINS[0], relayStates[0] ? HIGH : LOW);
      Vwire.virtualSend(V0, relayStates[0]);
      Serial.printf("[OEM] Manual toggle - Relay 1: %s\n", relayStates[0] ? "ON" : "OFF");
    }
    
    buttonPressed = false;
  }
}

void updateLED() {
  static unsigned long lastBlink = 0;
  static bool ledOn = false;
  
  if (Vwire.connected()) {
    // Solid on when connected
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (VwireProvision.isProvisioning()) {
    // Slow blink when in AP Mode
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      ledOn = !ledOn;
      digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
    }
  } else {
    // Fast blink when disconnected but provisioned
    if (wifiProvisioned && millis() - lastBlink > 100) {
      lastBlink = millis();
      ledOn = !ledOn;
      digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
    }
  }
}

/*
 * =============================================================================
 * OEM INTEGRATION GUIDE
 * =============================================================================
 * 
 * 1. ADMIN PANEL SETUP:
 *    - Log in to admin.vwire.io
 *    - Go to Devices > Provision Devices > Bulk Create
 *    - Enter number of devices and select device type (SWITCH, etc.)
 *    - Download the credentials CSV file (contains Device ID + Auth Token)
 * 
 * 2. FIRMWARE CUSTOMIZATION:
 *    - For each device, replace VWIRE_AUTH_TOKEN with the token from CSV
 *    - VWIRE_DEVICE_ID_DISPLAY is optional (for display/stickers only)
 *    - Update DEVICE_TYPE to match Admin Panel setting
 *    - Configure GPIO pins for your hardware
 *    - Compile and flash firmware
 * 
 *    CSV Format:
 *    deviceId,authToken,hardwareType,deviceType
 *    VW-ABC123,abc123xyz789...,ESP32,SWITCH
 * 
 * 3. PRODUCTION FLASHING:
 *    Option A: Manual per-device
 *      - Flash each device with unique AUTH TOKEN from CSV
 *    
 *    Option B: Automated script (recommended for >10 devices)
 *      - Use Python/Node.js script to read CSV and flash devices
 *      - Replace auth token at compile time using build flags:
 *        platformio run -e esp32 --target upload -DVWIRE_AUTH_TOKEN=\"token123\"
 * 
 *    Option C: OTA provisioning
 *      - Flash generic firmware first
 *      - Device fetches credentials from OEM server on first boot
 * 
 * 4. CUSTOMER EXPERIENCE:
 *    - Customer receives device with pre-configured auth token
 *    - Device ID sticker on packaging helps identify during claiming
 *    - Powers on device, connects to "VWire_VW-XXXXX" WiFi
 *    - Opens 192.168.4.1 in browser
 *    - Enters home WiFi credentials ONLY (auth token already in firmware)
 *    - Device connects to WiFi and Vwire Cloud
 *    - Customer opens Vwire app and claims device using Device ID
 *    SENSOR_TEMP:    Temperature display, humidity display
 *    SMART_PLUG:     Toggle, power monitoring display
 *    GENERIC:        Blank canvas for user customization
 * 
 * =============================================================================
 */
