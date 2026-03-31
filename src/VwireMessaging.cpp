/*
 * Vwire IOT Arduino Library - Messaging Helpers
 * 
 * Implements notification, alarm, email, and log helper methods used to send
 * application-facing events to the Vwire platform.
 * 
 * These helpers are optional and can be stripped from the build globally with
 * VWIRE_DISABLE_ALERTS when a sketch does not need them.
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "Vwire.h"

#if VWIRE_ENABLE_LOGGING
  #define VWIRE_LOG(message) _debugPrint(message)
  #define VWIRE_LOGF(...) _debugPrintf(__VA_ARGS__)
#else
  #define VWIRE_LOG(message) do { } while (0)
  #define VWIRE_LOGF(...) do { } while (0)
#endif

// =============================================================================
// NOTIFICATIONS
// =============================================================================

void VwireClass::notify(const char* message) {
#if VWIRE_ENABLE_ALERTS
  if (!connected()) return;
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/notify", _deviceId);
  unsigned int len = strlen(message);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(message);
  _mqttClient.endPublish();
  VWIRE_LOGF("[Vwire] Notify: %s", message);
#else
  (void)message;
#endif
}

void VwireClass::alarm(const char* message) {
  alarm(message, "default", 1, 50);
}

void VwireClass::alarm(const char* message, const char* sound) {
  alarm(message, sound, 1, 50);
}

void VwireClass::alarm(const char* message, const char* sound, uint8_t priority) {
  alarm(message, sound, priority, 50);
}

// =============================================================================
// ALARMS
// =============================================================================

void VwireClass::alarm(const char* message, const char* sound, uint8_t priority, uint8_t volume) {
#if VWIRE_ENABLE_ALERTS
  if (!connected()) return;

  static unsigned long lastAlarmId = 0;
  unsigned long alarmId = millis();
  if (alarmId == lastAlarmId) alarmId++;
  lastAlarmId = alarmId;
  if (volume > 100) volume = 100;

  char topic[96];
  char buffer[VWIRE_JSON_BUFFER_SIZE];

  snprintf(topic, sizeof(topic), "vwire/%s/alarm", _deviceId);
  snprintf(buffer, sizeof(buffer),
           "{\"type\":\"alarm\",\"message\":\"%s\",\"alarmId\":\"alarm_%lu\",\"sound\":\"%s\",\"priority\":%d,\"volume\":%d,\"timestamp\":%lu}",
           message, alarmId, sound, priority, volume, millis());

  unsigned int len = strlen(buffer);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(buffer);
  _mqttClient.endPublish();
  VWIRE_LOGF("[Vwire] Alarm: %s (sound: %s, priority: %d, volume: %d)", message, sound, priority, volume);
#else
  (void)message;
  (void)sound;
  (void)priority;
  (void)volume;
#endif
}

// =============================================================================
// EMAIL
// =============================================================================

void VwireClass::email(const char* subject, const char* body) {
#if VWIRE_ENABLE_ALERTS
  if (!connected()) return;

  char topic[96];
  char buffer[VWIRE_JSON_BUFFER_SIZE];

  snprintf(topic, sizeof(topic), "vwire/%s/email", _deviceId);
  snprintf(buffer, sizeof(buffer), "{\"subject\":\"%s\",\"body\":\"%s\"}", subject, body);

  unsigned int len = strlen(buffer);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(buffer);
  _mqttClient.endPublish();
  VWIRE_LOGF("[Vwire] Email: %s", subject);
#else
  (void)subject;
  (void)body;
#endif
}

// =============================================================================
// LOGGING
// =============================================================================

void VwireClass::log(const char* message) {
  if (!connected()) return;
  char topic[96];
  snprintf(topic, sizeof(topic), "vwire/%s/log", _deviceId);
  unsigned int len = strlen(message);
  _mqttClient.beginPublish(topic, len, false);
  _mqttClient.print(message);
  _mqttClient.endPublish();
}