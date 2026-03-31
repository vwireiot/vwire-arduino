/*
 * Vwire IOT Arduino Library - Reliable Delivery Addon Implementation
 * 
 * Implements application-level ACK tracking, retry handling, and delivery
 * result callbacks as an optional addon for the Vwire core.
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#include "VwireReliableDelivery.h"

#if VWIRE_ENABLE_LOGGING
  #define VWIRE_LOG(message) _vwire->_debugPrint(message)
  #define VWIRE_LOGF(...) _vwire->_debugPrintf(__VA_ARGS__)
#else
  #define VWIRE_LOG(message) do { } while (0)
  #define VWIRE_LOGF(...) do { } while (0)
#endif

// =============================================================================
// DEFAULT ADDON INSTANCE
// =============================================================================

static VwireReliableDeliveryAddon _vwireDefaultReliableDeliveryAddon;

// =============================================================================
// CONSTRUCTOR / REGISTRATION
// =============================================================================

VwireReliableDeliveryAddon::VwireReliableDeliveryAddon()
  : _vwire(nullptr)
  , _enabled(false)
  , _ackTimeout(VWIRE_DEFAULT_ACK_TIMEOUT)
  , _maxRetries(VWIRE_DEFAULT_MAX_RETRIES)
  , _deliveryCallback(nullptr)
  , _msgIdCounter(0) {
  memset(_pendingMessages, 0, sizeof(_pendingMessages));
}

void VwireReliableDeliveryAddon::begin(VwireClass& vwire) {
  _vwire = &vwire;
  vwire.addAddon(*this);
}

void VwireReliableDeliveryAddon::onAttach(VwireClass& vwire) {
  _vwire = &vwire;
}

// =============================================================================
// CONFIGURATION
// =============================================================================

void VwireReliableDeliveryAddon::setEnabled(bool enable) {
  _enabled = enable;
  if (!enable) {
    _clearPending();
  }
}

bool VwireReliableDeliveryAddon::isEnabled() const {
  return _enabled;
}

void VwireReliableDeliveryAddon::setAckTimeout(unsigned long timeout) {
  _ackTimeout = timeout;
}

void VwireReliableDeliveryAddon::setMaxRetries(uint8_t retries) {
  _maxRetries = retries;
}

void VwireReliableDeliveryAddon::onDeliveryStatus(DeliveryCallback cb) {
  _deliveryCallback = cb;
}

// =============================================================================
// STATUS / SEND
// =============================================================================

uint8_t VwireReliableDeliveryAddon::getPendingCount() const {
  uint8_t count = 0;
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (_pendingMessages[i].active) count++;
  }
  return count;
}

bool VwireReliableDeliveryAddon::isPending() const {
  return getPendingCount() > 0;
}

bool VwireReliableDeliveryAddon::send(uint8_t pin, const String& value) {
  if (!_enabled || !_vwire) {
    return false;
  }

  int slot = _findPendingSlot();
  if (slot < 0) {
    _vwire->_setError(VWIRE_ERR_QUEUE_FULL);
    VWIRE_LOG("[Vwire] Error: Reliable delivery queue full!");
    if (_deliveryCallback) {
      _deliveryCallback("queue_full", false);
    }
    return true;
  }

  PendingMessage& pending = _pendingMessages[slot];
  _msgIdCounter++;
  snprintf(pending.msgId, sizeof(pending.msgId), "%04X_%lu",
           (uint16_t)(_msgIdCounter & 0xFFFF), millis() % 10000);
  pending.pin = pin;
  if (value.length() >= sizeof(pending.value)) {
    VWIRE_LOGF("[Vwire] Warning: reliable delivery value truncated (%d > %d bytes)",
              value.length(), sizeof(pending.value) - 1);
  }
  strncpy(pending.value, value.c_str(), sizeof(pending.value) - 1);
  pending.value[sizeof(pending.value) - 1] = '\0';
  pending.sentAt = millis();
  pending.retries = 0;
  pending.active = true;

  _publishPending(pending);
  VWIRE_LOGF("[Vwire] Reliable write V%d = %s (msgId: %s)",
                       pin, value.c_str(), pending.msgId);
  return true;
}

// =============================================================================
// ADDON LIFECYCLE
// =============================================================================

void VwireReliableDeliveryAddon::onConnect() {
  if (!_enabled || !_vwire) return;

  String ackTopic = _vwire->_buildTopic("ack");
  _vwire->_mqttClient.subscribe(ackTopic.c_str(), 1);
  VWIRE_LOGF("[Vwire] Subscribed to: %s (ACK)", ackTopic.c_str());
}

bool VwireReliableDeliveryAddon::onMessage(const char* topic, const char* payload) {
  if (!_enabled) return false;

  const char* ackPos = strstr(topic, "/ack");
  if (!ackPos || *(ackPos + 4) != '\0') {
    return false;
  }

  const char* msgIdStart = strstr(payload, "\"msgId\":\"");
  const char* okStart = strstr(payload, "\"ok\":");
  if (!msgIdStart || !okStart) {
    return true;
  }

  msgIdStart += 9;
  const char* msgIdEnd = strchr(msgIdStart, '\"');
  if (!msgIdEnd) {
    return true;
  }

  char msgId[16];
  int len = min((int)(msgIdEnd - msgIdStart), 15);
  strncpy(msgId, msgIdStart, len);
  msgId[len] = '\0';

  bool success = strstr(okStart, "true") != nullptr;
  _handleAck(msgId, success);
  return true;
}

void VwireReliableDeliveryAddon::onRun() {
  if (!_enabled || !_vwire) return;

  unsigned long now = millis();
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    PendingMessage& pending = _pendingMessages[i];
    if (!pending.active) continue;
    if (now - pending.sentAt < _ackTimeout) continue;

    if (pending.retries < _maxRetries) {
      pending.retries++;
      pending.sentAt = now;
      _publishPending(pending);
      VWIRE_LOGF("[Vwire] Retry %d/%d for message %s",
                           pending.retries, _maxRetries, pending.msgId);
      continue;
    }

    VWIRE_LOGF("[Vwire] Message %s dropped after %d retries",
                         pending.msgId, _maxRetries);
    if (_deliveryCallback) {
      _deliveryCallback(pending.msgId, false);
    }
    pending.active = false;
  }
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

int VwireReliableDeliveryAddon::_findPendingSlot() const {
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    if (!_pendingMessages[i].active) return i;
  }
  return -1;
}

void VwireReliableDeliveryAddon::_clearPending() {
  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    _pendingMessages[i].active = false;
  }
}

void VwireReliableDeliveryAddon::_handleAck(const char* msgId, bool success) {
  VWIRE_LOGF("[Vwire] ACK received: %s = %s", msgId, success ? "OK" : "FAIL");

  for (int i = 0; i < VWIRE_MAX_PENDING_MESSAGES; i++) {
    PendingMessage& pending = _pendingMessages[i];
    if (!pending.active || strcmp(pending.msgId, msgId) != 0) continue;

    pending.active = false;
    if (_deliveryCallback) {
      _deliveryCallback(msgId, success);
    }
    return;
  }

  VWIRE_LOGF("[Vwire] ACK for unknown message: %s", msgId);
}

void VwireReliableDeliveryAddon::_publishPending(PendingMessage& message) {
  char payload[VWIRE_JSON_BUFFER_SIZE];
  char topic[96];

  snprintf(payload, sizeof(payload),
           "{\"msgId\":\"%s\",\"pin\":\"V%d\",\"value\":\"%s\"}",
           message.msgId, message.pin, message.value);
  snprintf(topic, sizeof(topic), "vwire/%s/data", _vwire->_deviceId);

  unsigned int len = strlen(payload);
  _vwire->_mqttClient.beginPublish(topic, len, false);
  _vwire->_mqttClient.print(payload);
  _vwire->_mqttClient.endPublish();
}

// =============================================================================
// VwireClass BRIDGE METHODS
// =============================================================================

bool VwireClass::_ensureReliableDeliveryAddon() {
#if VWIRE_ENABLE_RELIABLE_DELIVERY
  if (_reliableDeliveryAddon) {
    return true;
  }

  _reliableDeliveryAddon = &_vwireDefaultReliableDeliveryAddon;
  _vwireDefaultReliableDeliveryAddon.begin(*this);
  return true;
#else
  return false;
#endif
}

void VwireClass::setReliableDelivery(bool enable) {
#if VWIRE_ENABLE_RELIABLE_DELIVERY
  if (!enable) {
    if (_reliableDeliveryAddon) {
      _reliableDeliveryAddon->setEnabled(false);
    }
    _debugPrint("[Vwire] Reliable delivery: DISABLED");
    return;
  }

  if (_ensureReliableDeliveryAddon()) {
    _reliableDeliveryAddon->setEnabled(true);
    _debugPrint("[Vwire] Reliable delivery: ENABLED");
  }
#else
  (void)enable;
  _debugPrint("[Vwire] Reliable delivery is disabled in this build");
#endif
}

void VwireClass::setAckTimeout(unsigned long timeout) {
  if (_ensureReliableDeliveryAddon()) {
    _reliableDeliveryAddon->setAckTimeout(timeout);
  }
}

void VwireClass::setMaxRetries(uint8_t retries) {
  if (_ensureReliableDeliveryAddon()) {
    _reliableDeliveryAddon->setMaxRetries(retries);
  }
}

void VwireClass::onDeliveryStatus(DeliveryCallback cb) {
  if (_ensureReliableDeliveryAddon()) {
    _reliableDeliveryAddon->onDeliveryStatus(cb);
  }
}

uint8_t VwireClass::getPendingCount() {
  return _reliableDeliveryAddon ? _reliableDeliveryAddon->getPendingCount() : 0;
}

bool VwireClass::isDeliveryPending() {
  return _reliableDeliveryAddon ? _reliableDeliveryAddon->isPending() : false;
}
