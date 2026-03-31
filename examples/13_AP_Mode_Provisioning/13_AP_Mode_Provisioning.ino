/*
 * Vwire IOT - AP Mode Provisioning Example
 *
 * Simple flow:
 * 1. If credentials are already stored, connect with them.
 * 2. Otherwise start AP Mode and open the provisioning portal.
 * 3. After provisioning succeeds, connect to Vwire.
 */

#include <Vwire.h>
#include <VwireProvisioning.h>

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

#define AP_PASSWORD "vwire123"
#define AP_TIMEOUT 300000UL

unsigned long lastBlink = 0;
bool ledState = false;
bool vwireStarted = false;

VWIRE_RECEIVE(V0) {
  bool value = param.asBool();
  digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  Serial.printf("V0 -> LED: %s\n", value ? "ON" : "OFF");
}

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT");
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Vwire.syncVirtual(V0);
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected from Vwire IOT");
}

void onProvisioningState(VwireProvisioningState state) {
  switch (state) {
    case VWIRE_PROV_AP_ACTIVE:
      Serial.println("\nAP Mode active");
      Serial.printf("Connect to WiFi: %s\n", VwireProvision.getAPSSID());
      Serial.printf("Password: %s\n", AP_PASSWORD);
      Serial.printf("Open: http://%s\n\n", VwireProvision.getAPIP().c_str());
      break;

    case VWIRE_PROV_CONNECTING:
      Serial.println("[Provision] Connecting to WiFi...");
      break;

    case VWIRE_PROV_SUCCESS:
      Serial.println("[Provision] Saved credentials successfully");
      break;

    case VWIRE_PROV_FAILED:
      Serial.println("[Provision] Connection failed, portal stays available for retry");
      break;

    case VWIRE_PROV_TIMEOUT:
      Serial.println("[Provision] AP Mode timed out, restarting portal...");
      VwireProvision.startAPMode(AP_PASSWORD, AP_TIMEOUT);
      break;

    default:
      break;
  }
}

void onCredentialsReceived(const char* ssid, const char* password, const char* token) {
  Serial.println("[Provision] Received configuration");
  Serial.printf("SSID: %s\n", ssid);
  Serial.println("Password: [hidden]");
  Serial.printf("Token: %s\n", token);
}

void startProvisioningPortal() {
  Serial.println("Starting AP provisioning portal");

  // Optional provisioning logs:
  // VwireProvision.logTo(Serial);

  VwireProvision.onStateChange(onProvisioningState);
  VwireProvision.onCredentialsReceived(onCredentialsReceived);
  VwireProvision.startAPMode(AP_PASSWORD, AP_TIMEOUT);
}

bool connectWithStoredCredentials() {
  Serial.println("Using stored credentials");
  Serial.printf("SSID: %s\n", VwireProvision.getSSID());

  Vwire.config(VwireProvision.getAuthToken());

  // Optional client logs:
  // Vwire.logTo(Serial);

  if (Vwire.begin(VwireProvision.getSSID(), VwireProvision.getPassword())) {
    vwireStarted = true;
    return true;
  }

  Serial.println("Stored credentials failed, clearing and returning to AP Mode");
  VwireProvision.clearCredentials();
  startProvisioningPortal();
  return false;
}

void blinkWhileProvisioning() {
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("\nVwire IOT - AP Mode Provisioning");
  Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());

  if (VwireProvision.hasCredentials()) {
    connectWithStoredCredentials();
  } else {
    startProvisioningPortal();
  }
}

void loop() {
  if (VwireProvision.isProvisioning()) {
    VwireProvision.run();
    blinkWhileProvisioning();
    return;
  }

  if (!vwireStarted && VwireProvision.getState() == VWIRE_PROV_SUCCESS) {
    Serial.println("Provisioning complete, connecting to Vwire");
    Vwire.config(VwireProvision.getAuthToken());
    if (Vwire.begin()) {
      vwireStarted = true;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  if (vwireStarted) {
    Vwire.run();
  }
}
