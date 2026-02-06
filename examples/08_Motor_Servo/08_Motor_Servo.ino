/*
 * Vwire IOT - Motor/Servo Control
 * Copyright (c) 2026 Vwire IOT
 * 
 * Board: Any WiFi-capable board
 * 
 * Control DC motors and servo motors from dashboard:
 * - DC motor speed and direction
 * - Servo position control
 * - Joystick control for dual motors (robot)
 * 
 * Dashboard Setup:
 * - V0: Slider (Motor A speed, -100 to 100)
 * - V1: Slider (Motor B speed, -100 to 100)
 * - V2: Joystick (X,Y for dual motor control)
 * - V3: Slider (Servo 1 angle, 0-180)
 * - V4: Slider (Servo 2 angle, 0-180)
 * - V5: Button (Emergency stop)
 * 
 * Hardware:
 * - L298N or similar motor driver
 * - DC motors on ENA/IN1/IN2 and ENB/IN3/IN4
 * - Servo motors on GPIO
 */

#include <Vwire.h>

#if defined(ESP32)
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
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
// Transport protocol options:
// - VWIRE_TRANSPORT_TCP_SSL (port 8883) - MQTT over TLS/SSL - RECOMMENDED
// - VWIRE_TRANSPORT_TCP     (port 1883) - Plain MQTT (for boards without SSL support)
const VwireTransport TRANSPORT = VWIRE_TRANSPORT_TCP_SSL;

// =============================================================================
// PIN DEFINITIONS
// =============================================================================
#if defined(ESP32)
  // Motor A
  #define MOTOR_A_EN   25
  #define MOTOR_A_IN1  26
  #define MOTOR_A_IN2  27
  
  // Motor B
  #define MOTOR_B_EN   14
  #define MOTOR_B_IN1  12
  #define MOTOR_B_IN2  13
  
  // Servos
  #define SERVO1_PIN   32
  #define SERVO2_PIN   33
  
  // PWM channels for ESP32
  #define PWM_CHANNEL_A  0
  #define PWM_CHANNEL_B  1
  
#elif defined(ESP8266)
  // Motor A
  #define MOTOR_A_EN   D1
  #define MOTOR_A_IN1  D2
  #define MOTOR_A_IN2  D3
  
  // Motor B  
  #define MOTOR_B_EN   D5
  #define MOTOR_B_IN1  D6
  #define MOTOR_B_IN2  D7
  
  // Servos
  #define SERVO1_PIN   D4
  #define SERVO2_PIN   D8
  
#else
  // Generic Arduino
  #define MOTOR_A_EN   9
  #define MOTOR_A_IN1  8
  #define MOTOR_A_IN2  7
  #define MOTOR_B_EN   10
  #define MOTOR_B_IN1  11
  #define MOTOR_B_IN2  12
  #define SERVO1_PIN   5
  #define SERVO2_PIN   6
#endif

// =============================================================================
// OBJECTS
// =============================================================================
Servo servo1;
Servo servo2;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================
int motorASpeed = 0;  // -100 to 100
int motorBSpeed = 0;  // -100 to 100
int servo1Angle = 90;
int servo2Angle = 90;
bool emergencyStop = false;

// =============================================================================
// MOTOR CONTROL FUNCTIONS
// =============================================================================

void setupMotors() {
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  
  #if defined(ESP32)
    // Setup PWM channels for ESP32
    ledcSetup(PWM_CHANNEL_A, 5000, 8);
    ledcSetup(PWM_CHANNEL_B, 5000, 8);
    ledcAttachPin(MOTOR_A_EN, PWM_CHANNEL_A);
    ledcAttachPin(MOTOR_B_EN, PWM_CHANNEL_B);
  #else
    pinMode(MOTOR_A_EN, OUTPUT);
    pinMode(MOTOR_B_EN, OUTPUT);
  #endif
  
  stopMotors();
}

void setMotorA(int speed) {
  if (emergencyStop) speed = 0;
  motorASpeed = constrain(speed, -100, 100);
  
  // Map speed to PWM (0-255)
  int pwm = map(abs(motorASpeed), 0, 100, 0, 255);
  
  // Set direction
  if (motorASpeed > 0) {
    digitalWrite(MOTOR_A_IN1, HIGH);
    digitalWrite(MOTOR_A_IN2, LOW);
  } else if (motorASpeed < 0) {
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
  } else {
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, LOW);
  }
  
  // Set speed
  #if defined(ESP32)
    ledcWrite(PWM_CHANNEL_A, pwm);
  #else
    analogWrite(MOTOR_A_EN, pwm);
  #endif
  
  Serial.printf("Motor A: %d%% (PWM: %d)\n", motorASpeed, pwm);
}

void setMotorB(int speed) {
  if (emergencyStop) speed = 0;
  motorBSpeed = constrain(speed, -100, 100);
  
  int pwm = map(abs(motorBSpeed), 0, 100, 0, 255);
  
  if (motorBSpeed > 0) {
    digitalWrite(MOTOR_B_IN1, HIGH);
    digitalWrite(MOTOR_B_IN2, LOW);
  } else if (motorBSpeed < 0) {
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, HIGH);
  } else {
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, LOW);
  }
  
  #if defined(ESP32)
    ledcWrite(PWM_CHANNEL_B, pwm);
  #else
    analogWrite(MOTOR_B_EN, pwm);
  #endif
  
  Serial.printf("Motor B: %d%% (PWM: %d)\n", motorBSpeed, pwm);
}

void stopMotors() {
  setMotorA(0);
  setMotorB(0);
}

// =============================================================================
// SERVO CONTROL
// =============================================================================

void setupServos() {
  #if defined(ESP32)
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
  #endif
  
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  
  servo1.write(servo1Angle);
  servo2.write(servo2Angle);
}

void setServo1(int angle) {
  servo1Angle = constrain(angle, 0, 180);
  servo1.write(servo1Angle);
  Serial.printf("Servo 1: %d deg\n", servo1Angle);
}

void setServo2(int angle) {
  servo2Angle = constrain(angle, 0, 180);
  servo2.write(servo2Angle);
  Serial.printf("Servo 2: %d deg\n", servo2Angle);
}

// =============================================================================
// JOYSTICK TO DIFFERENTIAL DRIVE
// =============================================================================

void joystickToDifferential(int x, int y) {
  // Convert joystick X,Y (-100 to 100) to differential drive
  // X = turn, Y = throttle
  
  // Mix for tank-style driving
  int left = y + x;
  int right = y - x;
  
  // Constrain
  left = constrain(left, -100, 100);
  right = constrain(right, -100, 100);
  
  setMotorA(left);
  setMotorB(right);
}

// =============================================================================
// VIRTUAL PIN HANDLERS
// =============================================================================

VWIRE_RECEIVE(V0) {
  setMotorA(param.asInt());
}

VWIRE_RECEIVE(V1) {
  setMotorB(param.asInt());
}

VWIRE_RECEIVE(V2) {
  // Joystick sends X,Y as comma-separated values
  if (param.getArraySize() >= 2) {
    int x = param.getArrayItemInt(0);  // Left/Right
    int y = param.getArrayItemInt(1);  // Forward/Backward
    
    Serial.printf("Joystick: X=%d, Y=%d\n", x, y);
    joystickToDifferential(x, y);
  }
}

VWIRE_RECEIVE(V3) {
  setServo1(param.asInt());
}

VWIRE_RECEIVE(V4) {
  setServo2(param.asInt());
}

VWIRE_RECEIVE(V5) {
  emergencyStop = param.asBool();
  
  if (emergencyStop) {
    stopMotors();
    Vwire.notify("EMERGENCY STOP ACTIVATED!");
    Serial.println("!!! EMERGENCY STOP !!!");
  } else {
    Serial.println("Emergency stop released");
  }
}

// =============================================================================
// CONNECTION HANDLERS
// =============================================================================

VWIRE_CONNECTED() {
  Serial.println("Connected to Vwire IOT!");
  
  // Sync current state
  Vwire.virtualSend(V0, motorASpeed);
  Vwire.virtualSend(V1, motorBSpeed);
  Vwire.virtualSend(V3, servo1Angle);
  Vwire.virtualSend(V4, servo2Angle);
  Vwire.virtualSend(V5, emergencyStop);
}

VWIRE_DISCONNECTED() {
  Serial.println("Disconnected!");
  
  // Safety: Stop motors on disconnect
  stopMotors();
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("================================");
  Serial.println("  Vwire IOT - Motor/Servo Control");
  Serial.println("================================\n");
  
  // Initialize motors
  setupMotors();
  Serial.println("Motors initialized");
  
  // Initialize servos
  setupServos();
  Serial.println("Servos initialized");
  
  // Configure Vwire (uses default server: mqtt.vwire.io)
  Vwire.setDebug(true);
  Vwire.config(AUTH_TOKEN);
  Vwire.setDeviceId(DEVICE_ID);  // Use Device ID for MQTT topics
  Vwire.setTransport(TRANSPORT);
  
  // Connect (handlers auto-registered via VWIRE_RECEIVE macros)
  Vwire.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  Vwire.run();
  
  // Safety check: Stop motors if no connection for too long
  static unsigned long lastConnectedTime = 0;
  const unsigned long SAFETY_TIMEOUT = 10000;  // 10 seconds
  
  if (Vwire.connected()) {
    lastConnectedTime = millis();
  } else if (millis() - lastConnectedTime > SAFETY_TIMEOUT) {
    if (motorASpeed != 0 || motorBSpeed != 0) {
      Serial.println("Safety timeout: Stopping motors");
      stopMotors();
    }
  }
}
