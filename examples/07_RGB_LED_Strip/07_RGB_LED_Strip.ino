/*
 * Vwire IOT - RGB LED Strip Control
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: ESP32 (recommended) or ESP8266
 * 
 * Control an RGB LED strip with color picker and effects:
 * - Color picker (zeRGBa widget)
 * - Brightness control
 * - Effect selection (static, fade, rainbow, etc.)
 * - Power on/off
 * 
 * Dashboard Setup:
 * - V0: Button (Power on/off)
 * - V1: zeRGBa widget (Color picker, merge output)
 * - V2: Slider (Brightness, 0-255)
 * - V3: Menu/Segmented (Effect select: Static, Fade, Rainbow, etc.)
 * - V4: Slider (Effect speed, 1-100)
 * 
 * Hardware:
 * - WS2812B / NeoPixel LED strip
 * - Data pin on GPIO 5
 */

#include <Vwire.h>

// Uncomment ONE of these based on your LED library
#define USE_FASTLED
// #define USE_ADAFRUIT_NEOPIXEL

#ifdef USE_FASTLED
  #include <FastLED.h>
#endif

#ifdef USE_ADAFRUIT_NEOPIXEL
  #include <Adafruit_NeoPixel.h>
#endif

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
// LED STRIP CONFIGURATION
// =============================================================================
#define LED_PIN       5
#define NUM_LEDS      30
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB

// =============================================================================
// LED OBJECTS
// =============================================================================
#ifdef USE_FASTLED
  CRGB leds[NUM_LEDS];
#endif

#ifdef USE_ADAFRUIT_NEOPIXEL
  Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
#endif

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
bool powerOn = true;
uint8_t red = 255, green = 0, blue = 0;
uint8_t brightness = 128;
uint8_t currentEffect = 0;
uint8_t effectSpeed = 50;

// Effect names
const char* EFFECTS[] = {"Static", "Fade", "Rainbow", "Chase", "Twinkle", "Fire"};
const int NUM_EFFECTS = 6;

// Animation state
uint8_t hue = 0;
int chasePosition = 0;
unsigned long lastEffectUpdate = 0;

// =============================================================================
// LED FUNCTIONS
// =============================================================================

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
  #ifdef USE_FASTLED
    fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
  #endif
}

void setLED(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= NUM_LEDS) return;
  
  #ifdef USE_FASTLED
    leds[index] = CRGB(r, g, b);
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    strip.setPixelColor(index, strip.Color(r, g, b));
  #endif
}

void showLEDs() {
  #ifdef USE_FASTLED
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    strip.show();
  #endif
}

void setBrightness(uint8_t b) {
  #ifdef USE_FASTLED
    FastLED.setBrightness(b);
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    strip.setBrightness(b);
    strip.show();
  #endif
}

void clearLEDs() {
  #ifdef USE_FASTLED
    FastLED.clear();
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    strip.clear();
    strip.show();
  #endif
}

// =============================================================================
// EFFECTS
// =============================================================================

void effectStatic() {
  setAllLEDs(red, green, blue);
}

void effectFade() {
  static uint8_t fadeValue = 0;
  static bool fadeDirection = true;
  
  float factor = fadeValue / 255.0;
  setAllLEDs(red * factor, green * factor, blue * factor);
  
  if (fadeDirection) {
    fadeValue += 5;
    if (fadeValue >= 250) fadeDirection = false;
  } else {
    fadeValue -= 5;
    if (fadeValue <= 5) fadeDirection = true;
  }
}

void effectRainbow() {
  #ifdef USE_FASTLED
    fill_rainbow(leds, NUM_LEDS, hue, 7);
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    for (int i = 0; i < NUM_LEDS; i++) {
      int pixelHue = (hue + (i * 65536L / NUM_LEDS)) % 65536;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
  #endif
  
  hue++;
}

void effectChase() {
  clearLEDs();
  
  for (int i = 0; i < 3; i++) {
    int pos = (chasePosition + i) % NUM_LEDS;
    setLED(pos, red, green, blue);
  }
  showLEDs();
  
  chasePosition = (chasePosition + 1) % NUM_LEDS;
}

void effectTwinkle() {
  // Fade all LEDs slightly
  #ifdef USE_FASTLED
    fadeToBlackBy(leds, NUM_LEDS, 20);
  #endif
  
  // Add random sparkles
  if (random(10) < 3) {
    int pos = random(NUM_LEDS);
    setLED(pos, red, green, blue);
  }
  showLEDs();
}

void effectFire() {
  #ifdef USE_FASTLED
    // Simple fire effect
    for (int i = 0; i < NUM_LEDS; i++) {
      int flicker = random(0, 150);
      leds[i] = CRGB(255 - flicker, 80 - (flicker / 2), 0);
    }
    FastLED.show();
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    for (int i = 0; i < NUM_LEDS; i++) {
      int flicker = random(0, 150);
      strip.setPixelColor(i, strip.Color(255 - flicker, 80 - (flicker / 2), 0));
    }
    strip.show();
  #endif
}

void runEffect() {
  if (!powerOn) {
    clearLEDs();
    return;
  }
  
  unsigned long interval = map(effectSpeed, 1, 100, 100, 10);
  if (millis() - lastEffectUpdate < interval) return;
  lastEffectUpdate = millis();
  
  switch (currentEffect) {
    case 0: effectStatic(); break;
    case 1: effectFade(); break;
    case 2: effectRainbow(); break;
    case 3: effectChase(); break;
    case 4: effectTwinkle(); break;
    case 5: effectFire(); break;
    default: effectStatic(); break;
  }
}

// =============================================================================
// VIRTUAL PIN HANDLERS (Auto-registered via macros)
// =============================================================================

VWIRE_RECEIVE(V0) {
  powerOn = param.asBool();
  
  if (!powerOn) {
    clearLEDs();
  }
  
  Serial.printf("Power: %s\n", powerOn ? "ON" : "OFF");
}

VWIRE_RECEIVE(V1) {
  // zeRGBa sends comma-separated RGB values
  if (param.getArraySize() >= 3) {
    red = param.getArrayItemInt(0);
    green = param.getArrayItemInt(1);
    blue = param.getArrayItemInt(2);
  }
  
  Serial.printf("Color: R=%d G=%d B=%d\n", red, green, blue);
}

VWIRE_RECEIVE(V2) {
  brightness = param.asInt();
  setBrightness(brightness);
  Serial.printf("Brightness: %d\n", brightness);
}

VWIRE_RECEIVE(V3) {
  currentEffect = param.asInt();
  if (currentEffect >= NUM_EFFECTS) currentEffect = 0;
  Serial.printf("Effect: %s\n", EFFECTS[currentEffect]);
}

VWIRE_RECEIVE(V4) {
  effectSpeed = param.asInt();
  Serial.printf("Effect Speed: %d\n", effectSpeed);
}

// =============================================================================
// CONNECTION HANDLERS (Auto-registered via macros)
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Sync current state
  Vwire.virtualSend(V0, powerOn);
  Vwire.virtualSendf(V1, "%d,%d,%d", red, green, blue);
  Vwire.virtualSend(V2, brightness);
  Vwire.virtualSend(V3, currentEffect);
  Vwire.virtualSend(V4, effectSpeed);
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
  Serial.println("  Vwire IOT - RGB LED Control");
  Serial.println("================================\n");
  
  // Initialize LEDs
  #ifdef USE_FASTLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness);
    Serial.println("Using FastLED library");
  #endif
  
  #ifdef USE_ADAFRUIT_NEOPIXEL
    strip.begin();
    strip.setBrightness(brightness);
    strip.show();
    Serial.println("Using Adafruit NeoPixel library");
  #endif
  
  Serial.printf("LED Strip: %d LEDs on GPIO %d\n", NUM_LEDS, LED_PIN);
  
  // Test pattern
  setAllLEDs(255, 0, 0); delay(300);
  setAllLEDs(0, 255, 0); delay(300);
  setAllLEDs(0, 0, 255); delay(300);
  clearLEDs();
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  // Note: VWIRE_RECEIVE(), VWIRE_CONNECTED(), and VWIRE_DISCONNECTED() macros
  // automatically register handlers - no manual registration needed!
  
  // Connect
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
  runEffect();
}
