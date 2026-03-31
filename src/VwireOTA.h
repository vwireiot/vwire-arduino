/*
 * Vwire IOT Arduino Library - OTA Addon
 * 
 * Provides modular local OTA and Cloud OTA support for boards that can update
 * firmware over WiFi.
 * 
 * Usage:
 *   #include <Vwire.h>
 *   #include <VwireOTA.h>
 * 
 *   // Simple sketch usage:
 *   Vwire.enableOTA("my-device");
 *   Vwire.enableCloudOTA();
 * 
 *   // Advanced usage with explicit addon ownership:
 *   VwireOTAAddon ota;
 *   ota.begin(Vwire);
 * 
 * Copyright (c) 2026 Vwire IOT
 * Website: https://vwire.io
 * MIT License
 */

#ifndef VWIRE_OTA_H
#define VWIRE_OTA_H

#include <Arduino.h>
#include "Vwire.h"

// =============================================================================
// VWIRE OTA ADDON
// =============================================================================

/**
 * @brief Modular OTA feature implementation for the Vwire core
 *
 * Handles:
 * - Local OTA via ArduinoOTA on supported boards
 * - Cloud OTA commands received over MQTT
 * - OTA status publishing back to the Vwire platform
 */

class VwireOTAAddon : public VwireOTAFeature {
public:
  /** @brief Constructor */
  VwireOTAAddon();

  /** @brief Register this addon with the Vwire core */
  void begin(VwireClass& vwire);

  // =========================================================================
  // ADDON LIFECYCLE
  // =========================================================================

  void onAttach(VwireClass& vwire) override;
  void onConnect() override;
  bool onMessage(const char* topic, const char* payload) override;
  void onRun() override;

  #if VWIRE_HAS_OTA
  // =========================================================================
  // LOCAL OTA
  // =========================================================================

  /** @brief Enable classic Arduino OTA support */
  void enableLocal(const char* hostname, const char* password) override;

  /** @brief Process local OTA requests */
  void handle() override;
  #endif

  #if VWIRE_ENABLE_CLOUD_OTA
  // =========================================================================
  // CLOUD OTA
  // =========================================================================

  /** @brief Enable Cloud OTA handling for this session */
  void enableCloud() override;

  /** @brief Disable Cloud OTA handling for this session */
  void disableCloud() override;

  /** @brief Check whether Cloud OTA is currently enabled */
  bool isCloudEnabled() const override;
  #endif

private:
  VwireClass* _vwire;

  #if VWIRE_ENABLE_LOCAL_OTA
  bool _localEnabled;
  #endif

  #if VWIRE_ENABLE_CLOUD_OTA
  bool _cloudEnabled;
  void _handleCloudOTA(const char* payload);
  void _ensureMqttForOTA();
  void _publishOTAStatus(const char* updateId, const char* status, int progress,
                         const char* error = nullptr);
  #endif
};

#endif // VWIRE_OTA_H