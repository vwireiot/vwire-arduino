/*
 * Vwire IOT - Analog Pin Control Example
 * 
 * Demonstrates reading analog sensors and controlling PWM outputs
 * using VWire's GPIO pin management — all configurable from the cloud.
 * 
 * Works with any WiFi-capable board:
 * - ESP32, ESP8266, Arduino WiFi, RP2040-W, etc.
 * 
 * Features demonstrated:
 * - ANALOG_INPUT mode for ADC readings
 * - OUTPUT mode with smart write: slider (0-255) auto-PWMs
 * - Mixed virtual + analog + digital pins in one sketch
 * - Configurable read intervals from the dashboard
 * 
 * Smart Write for OUTPUT pins:
 *   value 0 or 1   → digitalWrite (on/off)
 *   value 2 – 255  → analogWrite / PWM (brightness, speed, etc.)
 * Works identically on ESP32, ESP8266 and Arduino — platform
 * differences (ledcWrite, PWMRANGE) are handled automatically.
 * 
 * Dashboard Setup:
 * 1. Create a device in the VWire dashboard
 * 2. Add pins:
 *    - A0 (ANALOG_INPUT) — potentiometer / light sensor
 *    - A1 (ANALOG_INPUT) — temperature sensor (TMP36 or similar)
 *    - D5 (OUTPUT)       — LED brightness control (slider 0-255)
 *    - V0 (virtual)      — combined status string
 * 3. Configure pin modes in pin edit modal → GPIO Configuration
 * 4. Add widgets:
 *    - Gauge widget  → pin A0  (0–4095 range)
 *    - Value display → pin A1 (shows raw ADC)
 *    - Slider widget → pin D5 (0–255, auto-PWM)
 *    - Label widget  → pin V0 (status text)
 * 
 * Wiring (ESP32 example):
 * - GPIO 36 (ADC1_0) → Potentiometer wiper
 * - GPIO 39 (ADC1_3) → TMP36 VOUT
 * - GPIO  5          → LED + 220Ω to GND  (PWM via smart write)
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

// Timer for sending virtual pin status
VwireTimer statusTimer;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n=== Vwire IOT — Analog Pin Control Example ===\n");
  
  // 1. Configure connection
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Device ID used for MQTT topics (required)
  Vwire.setTransport(TRANSPORT);
  
  // 2. Enable GPIO management
  Vwire.enableGPIO();
  
  // 3. (Optional) Pre-register pins locally for development / offline use.
  //    Pin names map directly to board labels — no GPIO numbers needed!
  //    A0/A1 auto-resolve to the correct analog channel on each platform.
  //    D5 as OUTPUT auto-handles both digital (0/1) and PWM (2-255).
  //    When the device connects, the cloud config overrides these.
  // Vwire.addGPIOPin("A0", VWIRE_GPIO_ANALOG_INPUT, 500);   // Pot (read every 500ms)
  // Vwire.addGPIOPin("A1", VWIRE_GPIO_ANALOG_INPUT, 2000);  // Temp sensor
  // Vwire.addGPIOPin("D5", VWIRE_GPIO_OUTPUT);              // LED brightness (smart write)
  
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
  
  // 6. Send a status string on virtual pin V0 every 10 seconds
  statusTimer.setInterval(10000L, []() {
    int potValue = Vwire.gpioRead("A0");
    int tempRaw  = Vwire.gpioRead("A1");
    
    // Convert TMP36 raw ADC to °C (3.3V reference, 12-bit ADC)
    float voltage = (tempRaw / 4095.0) * 3.3;
    float tempC   = (voltage - 0.5) * 100.0;
    
    // Send formatted status to virtual pin
    Vwire.virtualSendf(V0, "Pot:%d Temp:%.1fC", potValue, tempC);
    
    Serial.printf("Status → Pot: %d, Temp: %.1f°C\n", potValue, tempC);
  });
}

// Handle slider commands from dashboard for LED brightness
// (GPIO manager handles D5 automatically, but you can also use VWIRE_RECEIVE
//  for virtual pins alongside GPIO pins)
VWIRE_CONNECTED() {
  Serial.println("Connected! GPIO pins managed: " + String(Vwire.getGPIO().getPinCount()));
}

void loop() {
  Vwire.run();
  statusTimer.run();
  
  // The GPIO manager automatically:
  // 1. Reads A0 every 500ms and publishes when the value changes
  // 2. Reads A1 every 2000ms and publishes when the value changes
  // 3. Applies slider commands from the dashboard to D5 (PWM)
}
