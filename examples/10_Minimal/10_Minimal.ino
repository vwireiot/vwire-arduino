/*
 * Vwire IOT - Minimal Example
 * 
 * The simplest possible Vwire IOT sketch.
 * Good starting point for new projects.
 * 
 * Copyright (c) 2026 Vwire IOT
 * MIT License
 */

#include <Vwire.h>

// Configuration - UPDATE THESE!
#define WIFI_SSID     "YOUR_WIFI"
#define WIFI_PASS     "YOUR_PASSWORD"
#define AUTH_TOKEN    "YOUR_AUTH_TOKEN"

void setup() {
  Serial.begin(115200);
  
  // Configure and connect (uses default Vwire server with TLS)
  Vwire.config(AUTH_TOKEN);
  Vwire.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  Vwire.run();
  
  // Send uptime every 5 seconds
  static unsigned long lastSend = 0;
  if (Vwire.connected() && millis() - lastSend > 5000) {
    lastSend = millis();
    Vwire.virtualSend(V0, millis() / 1000);
  }
}
