/*
 * Vwire IOT - Digital Pin Control Example
 * 
 * Demonstrates the GPIO pin management feature for direct digital pin
 * control from the VWire cloud dashboard — without writing custom handlers.
 * 
 * Works with any WiFi-capable board:
 * - ESP32, ESP8266/NodeMCU, Arduino WiFi, RP2040-W, etc.
 * 
 * Features demonstrated:
 * - enableGPIO() — automatic hardware pin management
 * - Cloud-configured pin modes (OUTPUT, INPUT, INPUT_PULLUP, PWM)
 * - Automatic reading of input pins at configurable intervals
 * - Pin names (D0, D1, ...) map directly to board labels
 * - On ESP8266/NodeMCU: D4 automatically maps to GPIO 2 (built-in LED)
 * - On ESP32: D4 maps to GPIO 4
 * 
 * Smart Write (Blynk-style):
 * OUTPUT pins automatically adapt based on the value sent:
 *   0 or 1   → digitalWrite (clean digital on/off)
 *   2 – 255  → analogWrite / PWM (proportional duty cycle)
 * A Switch widget (sends 0/1) toggles the pin on/off.
 * A Slider widget (sends 0-255) controls brightness/speed via PWM.
 * No separate pin mode needed — just use OUTPUT for both!
 * 
 * All platform differences are handled automatically:
 *   ESP32 Core 3.x — native analogWrite
 *   ESP32 Core 2.x — ledc API with lazy channel setup
 *   ESP8266        — analogWrite with PWMRANGE scaling
 *   Arduino (Uno, Mega) — standard analogWrite
 * 
 * Dashboard Setup:
 * 1. Create a device in the VWire dashboard
 * 2. Add pins: D2 (OUTPUT), D4 (INPUT_PULLUP), D5 (OUTPUT)
 * 3. Set pin modes in the pin edit modal → GPIO Configuration
 * 4. Add dashboard widgets:
 *    - Button widget  → pin D2  (controls relay, sends 0/1)
 *    - Slider widget  → pin D5  (dims LED, sends 0-255)
 *    - Switch widget  → pin D5  (toggles LED on/off, sends 0/1)
 *    - Value display  → pin D4  (reads button state)
 * 
 * That's it! The pin names on the dashboard match the pin labels on
 * your board. No need to look up GPIO numbers.
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
const char* AUTH_TOKEN    = "YOUR_AUTH_TOKEN";
const char* DEVICE_ID     = "YOUR_DEVICE_ID";  // VW-XXXXXX (OEM) or VU-XXXXXX (user-created)

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
// VWIRE_TRANSPORT_TCP_SSL (port 8883) - Encrypted, RECOMMENDED
// VWIRE_TRANSPORT_TCP     (port 1883) - Plain TCP, use if SSL not supported
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n=== Vwire IOT — Digital Pin Control Example ===\n");
  
  // 1. Configure connection
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Device ID used for MQTT topics (required)
  Vwire.setTransport(TRANSPORT);
  
  // 2. Enable GPIO management — this tells the library to:
  //    • Subscribe to the "pinconfig" MQTT topic
  //    • Automatically set up hardware pin modes from cloud settings
  //    • Poll input pins and publish state changes
  //    • Apply incoming commands to output pins (D* topics)
  Vwire.enableGPIO();
  
  // 3. (Optional) Pre-register pins locally for development / offline use.
  //    Pin names map directly to board labels — no GPIO numbers needed!
  //    On ESP8266/NodeMCU: D4 → GPIO 2 (built-in LED) automatically.
  //    When the device connects, the cloud config overrides these.
  // Vwire.addGPIOPin("D2",  VWIRE_GPIO_OUTPUT);                 // Relay
  // Vwire.addGPIOPin("D4",  VWIRE_GPIO_INPUT_PULLUP, 200);      // Button (read every 200ms)
  // Vwire.addGPIOPin("D5",  VWIRE_GPIO_OUTPUT);                 // LED
  
  // 4. Enable debug output (optional)
  Vwire.setDebug(true);
  
  // 5. Connect to WiFi and Vwire
  Serial.println("Connecting...");
  if (Vwire.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("Connection successful!");
  } else {
    Serial.println("Connection failed!");
    Serial.print("Error code: ");
    Serial.println(Vwire.getLastError());
  }
}

// You can still use VWIRE_CONNECTED / VWIRE_DISCONNECTED alongside GPIO:
VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  Serial.print("GPIO pins managed: ");
  Serial.println(Vwire.getGPIO().getPinCount());
}

void loop() {
  Vwire.run();  // Handles MQTT, heartbeat, and GPIO polling automatically
  
  // You can also read/write GPIO pins in your sketch:
  // int btn = Vwire.gpioRead("D4");    // Last polled value
  // Vwire.gpioWrite("D5", HIGH);       // Immediate digital write
  // Vwire.gpioWrite("D5", 128);        // PWM — ~50% duty cycle
  // Vwire.gpioSend("D2", 1);           // Publish to cloud
}
