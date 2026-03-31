/*
 * Vwire IOT Arduino Library - Reliable Delivery Addon
 * 
 * Provides application-level acknowledgment handling for sketches that need
 * guaranteed delivery semantics on top of normal MQTT publish behaviour.
 * 
 * Usage:
 *   Vwire.setReliableDelivery(true);
 *   Vwire.setAckTimeout(5000);
 *   Vwire.setMaxRetries(3);
 *   Vwire.onDeliveryStatus(onDeliveryResult);
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_RELIABLE_DELIVERY_H
#define VWIRE_RELIABLE_DELIVERY_H

#include <Arduino.h>
#include "Vwire.h"

// =============================================================================
// RELIABLE DELIVERY ADDON
// =============================================================================

/**
 * @brief Addon that tracks pending virtual writes until the server acknowledges them
 *
 * This module attaches to the Vwire core only when reliable delivery is
 * enabled, keeping the default code path lean for ordinary sketches.
 */

class VwireReliableDeliveryAddon : public VwireReliableDelivery {
public:
  /** @brief Constructor */
  VwireReliableDeliveryAddon();

  /** @brief Register this addon with the Vwire core */
  void begin(VwireClass& vwire);

  // =========================================================================
  // CONFIGURATION
  // =========================================================================

  void setEnabled(bool enable) override;
  bool isEnabled() const override;
  void setAckTimeout(unsigned long timeout) override;
  void setMaxRetries(uint8_t retries) override;
  void onDeliveryStatus(DeliveryCallback cb) override;

  // =========================================================================
  // STATUS / SEND
  // =========================================================================

  uint8_t getPendingCount() const override;
  bool isPending() const override;
  bool send(uint8_t pin, const String& value) override;

  // =========================================================================
  // ADDON LIFECYCLE
  // =========================================================================

  void onAttach(VwireClass& vwire) override;
  void onConnect() override;
  bool onMessage(const char* topic, const char* payload) override;
  void onRun() override;

private:
  /** @brief Internal queue entry for a reliable publish awaiting ACK */
  struct PendingMessage {
    char msgId[16];
    uint8_t pin;
    char value[64];
    unsigned long sentAt;
    uint8_t retries;
    bool active;
  };

  VwireClass* _vwire;
  bool _enabled;
  unsigned long _ackTimeout;
  uint8_t _maxRetries;
  DeliveryCallback _deliveryCallback;
  uint32_t _msgIdCounter;
  PendingMessage _pendingMessages[VWIRE_MAX_PENDING_MESSAGES];

  int _findPendingSlot() const;
  void _clearPending();
  void _handleAck(const char* msgId, bool success);
  void _publishPending(PendingMessage& message);
};

#endif // VWIRE_RELIABLE_DELIVERY_H