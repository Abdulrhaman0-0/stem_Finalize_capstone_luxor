// ============================================================================
// ESP32 TRAFFIC CONTROLLER NODE
// Spec v1.2 - Smart Traffic Intersection with MQTT
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMQTT_ESP32.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// ============================================================================
// STATE MACHINE
// ============================================================================
enum Mode {
  MODE_NORMAL,
  MODE_EMERGENCY,
  MODE_FAILSAFE
};

enum Phase {
  PHASE_A_GREEN,
  PHASE_B_GREEN
};

Mode currentMode = MODE_FAILSAFE;  // Start in failsafe until connected
Phase currentPhase = PHASE_A_GREEN;
Phase lastPhase = PHASE_A_GREEN;

// ============================================================================
// STATE VARIABLES
// ============================================================================
unsigned long phaseStartTime = 0;
unsigned long phaseEndTime = 0;
unsigned long emergencyStartTime = 0;
unsigned long lastDischargeTime = 0;
unsigned long lastStatePublishTime = 0;

int queueA = 0;
int queueB = 0;

unsigned long A_wait_start_ms = 0;
unsigned long B_wait_start_ms = 0;

// Emergency state
String emergencyApproach = "";

// ============================================================================
// LED & BUZZER CONTROL
// ============================================================================
void setLEDs(bool a_red, bool a_green, bool b_red, bool b_green) {
  digitalWrite(PIN_A_RED, a_red ? HIGH : LOW);
  digitalWrite(PIN_A_GREEN, a_green ? HIGH : LOW);
  digitalWrite(PIN_B_RED, b_red ? HIGH : LOW);
  digitalWrite(PIN_B_GREEN, b_green ? HIGH : LOW);
}

void setBuzzer(bool on) {
  if (on) {
#if defined(ARDUINO_ESP32_VERSION_MAJOR) && ARDUINO_ESP32_VERSION_MAJOR >= 3
    ledcWriteTone(PIN_BUZZER, BUZZER_FREQ);
    ledcWrite(PIN_BUZZER, 128);  // 50% duty
#else
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQ);
    ledcWrite(BUZZER_CHANNEL, 128);  // 50% duty
#endif
  } else {
#if defined(ARDUINO_ESP32_VERSION_MAJOR) && ARDUINO_ESP32_VERSION_MAJOR >= 3
    ledcWrite(PIN_BUZZER, 0);
#else
    ledcWrite(BUZZER_CHANNEL, 0);
#endif
  }
}

void setPhaseOutputs(Phase phase, bool buzzer) {
  if (phase == PHASE_A_GREEN) {
    setLEDs(false, true, true, false);  // A=GREEN, B=RED
  } else {
    setLEDs(true, false, false, true);  // A=RED, B=GREEN
  }
  setBuzzer(buzzer);
}

void setFailsafeOutputs() {
  setLEDs(true, false, true, false);  // All RED
  setBuzzer(false);
}

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================
void incrementQueue(char direction) {
  if (direction == 'A') {
    if (queueA < QUEUE_MAX) {
      queueA++;
      Serial.printf("[QUEUE] A: %d\n", queueA);
    } else {
      Serial.println("[QUEUE] WARNING: queue_A at max (50)");
    }
  } else if (direction == 'B') {
    if (queueB < QUEUE_MAX) {
      queueB++;
      Serial.printf("[QUEUE] B: %d\n", queueB);
    } else {
      Serial.println("[QUEUE] WARNING: queue_B at max (50)");
    }
  }
}

void dischargeQueues() {
  if (currentMode != MODE_NORMAL) return;
  
  unsigned long now = millis();
  if (now - lastDischargeTime >= DISCHARGE_TICK_MS) {
    lastDischargeTime = now;
    
    if (currentPhase == PHASE_A_GREEN && queueA > 0) {
      queueA--;
      Serial.printf("[DISCHARGE] A: %d remaining\n", queueA);
    } else if (currentPhase == PHASE_B_GREEN && queueB > 0) {
      queueB--;
      Serial.printf("[DISCHARGE] B: %d remaining\n", queueB);
    }
  }
}

// ============================================================================
// PHASE MANAGEMENT (SNAPSHOT TIMING)
// ============================================================================
void startPhase(Phase phase) {
  currentPhase = phase;
  phaseStartTime = millis();
  
  // Snapshot-based duration calculation
  int activeQueue = (phase == PHASE_A_GREEN) ? queueA : queueB;
  unsigned long duration = MIN_GREEN_MS + (activeQueue * EXTEND_STEP_MS);
  duration = constrain(duration, MIN_GREEN_MS, MAX_GREEN_MS);
  
  phaseEndTime = phaseStartTime + duration;
  
  // Set wait timer for direction that just turned RED
  if (phase == PHASE_A_GREEN) {
    B_wait_start_ms = millis();
    Serial.printf("[PHASE] A_GREEN started, duration=%lums, queue snapshot: A=%d B=%d\n", 
                  duration, queueA, queueB);
  } else {
    A_wait_start_ms = millis();
    Serial.printf("[PHASE] B_GREEN started, duration=%lums, queue snapshot: A=%d B=%d\n", 
                  duration, queueA, queueB);
  }
  
  setPhaseOutputs(phase, false);
  lastPhase = phase;
}

void checkAntiStarvation() {
  if (currentMode != MODE_NORMAL) return;
  
  unsigned long now = millis();
  unsigned long phaseElapsed = now - phaseStartTime;
  
  if (phaseElapsed < MIN_GREEN_MS) return;  // Must run at least MIN_GREEN
  
  if (currentPhase == PHASE_A_GREEN) {
    unsigned long B_waiting = now - B_wait_start_ms;
    if (B_waiting > MAX_WAIT_MS) {
      Serial.println("[ANTI-STARVATION] Forcing switch to B_GREEN");
      startPhase(PHASE_B_GREEN);
    }
  } else {
    unsigned long A_waiting = now - A_wait_start_ms;
    if (A_waiting > MAX_WAIT_MS) {
      Serial.println("[ANTI-STARVATION] Forcing switch to A_GREEN");
      startPhase(PHASE_A_GREEN);
    }
  }
}

void checkPhaseTimeout() {
  if (currentMode != MODE_NORMAL) return;
  
  if (millis() >= phaseEndTime) {
    // Phase timeout - switch
    if (currentPhase == PHASE_A_GREEN) {
      startPhase(PHASE_B_GREEN);
    } else {
      startPhase(PHASE_A_GREEN);
    }
  }
}

// ============================================================================
// MODE MANAGEMENT
// ============================================================================
void enterEmergency(String approach) {
  Serial.printf("[EMERGENCY] Preemption for approach %s\n", approach.c_str());
  
  currentMode = MODE_EMERGENCY;
  emergencyApproach = approach;
  emergencyStartTime = millis();
  
  // Set appropriate phase and buzzer
  if (approach == "A") {
    setPhaseOutputs(PHASE_A_GREEN, true);
    currentPhase = PHASE_A_GREEN;
  } else {
    setPhaseOutputs(PHASE_B_GREEN, true);
    currentPhase = PHASE_B_GREEN;
  }
}

void checkEmergencyTimeout() {
  if (currentMode != MODE_EMERGENCY) return;
  
  if (millis() - emergencyStartTime >= EMERGENCY_MS) {
    Serial.println("[EMERGENCY] Timeout - returning to NORMAL");
    currentMode = MODE_NORMAL;
    setBuzzer(false);
    
    // Resume with emergency approach GREEN
    startPhase(currentPhase);
  }
}

void enterFailsafe() {
  Serial.println("[FAILSAFE] Entering all-red state");
  currentMode = MODE_FAILSAFE;
  setFailsafeOutputs();
}

void resumeFromFailsafe() {
  Serial.println("[FAILSAFE] Reconnected - resuming NORMAL");
  currentMode = MODE_NORMAL;
  
  // Choose phase based on queues
  if (queueA > queueB) {
    startPhase(PHASE_A_GREEN);
  } else if (queueB > queueA) {
    startPhase(PHASE_B_GREEN);
  } else {
    // Tie: alternate from last
    startPhase(lastPhase == PHASE_A_GREEN ? PHASE_B_GREEN : PHASE_A_GREEN);
  }
  
  // Reset both wait timers
  A_wait_start_ms = millis();
  B_wait_start_ms = millis();
}

// ============================================================================
// MQTT MESSAGE HANDLERS
// ============================================================================
void handleEmergencyDetect(String payload) {
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.printf("[MQTT] Emergency JSON parse error: %s\n", error.c_str());
    return;
  }
  
  JsonObject& obj = doc.as<JsonObject>();
  String approach = obj["approach"] | "";
  if (approach != "A" && approach != "B") {
    Serial.println("[MQTT] Invalid approach in emergency");
    return;
  }
  
  if (currentMode == MODE_EMERGENCY) {
    if (approach == emergencyApproach) {
      // Same direction - reset timer
      emergencyStartTime = millis();
      Serial.println("[EMERGENCY] Same approach - timer reset");
    } else {
      // Different direction - switch
      enterEmergency(approach);
    }
  } else {
    enterEmergency(approach);
  }
}

void handleTrafficArrive(char direction, String payload) {
  // Arrive messages just increment queue (no team_id filtering per spec)
  incrementQueue(direction);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, 
                   size_t len, size_t index, size_t total) {
  String topicStr = String(topic);
  String payloadStr = String(payload).substring(0, len);
  
  Serial.printf("[MQTT] RX: %s | %s\n", topicStr.c_str(), payloadStr.c_str());
  
  if (topicStr == TOPIC_EMERGENCY_DETECT) {
    handleEmergencyDetect(payloadStr);
  } else if (topicStr == TOPIC_TRAFFIC_A_ARRIVE) {
    handleTrafficArrive('A', payloadStr);
  } else if (topicStr == TOPIC_TRAFFIC_B_ARRIVE) {
    handleTrafficArrive('B', payloadStr);
  }
}

// ============================================================================
// MQTT STATE PUBLISHER
// ============================================================================
void publishState() {
  if (!mqttClient.connected()) return;
  
  unsigned long now = millis();
  if (now - lastStatePublishTime < STATE_PUBLISH_MS) return;
  lastStatePublishTime = now;
  
  StaticJsonDocument<256> doc;
  JsonObject& obj = doc.to<JsonObject>();
  
  obj["phase"] = (currentPhase == PHASE_A_GREEN) ? "A_GREEN" : "B_GREEN";
  obj["remaining_ms"] = (currentMode == MODE_NORMAL) ? max(0L, (long)(phaseEndTime - now)) : 0;
  obj["queue_A"] = queueA;
  obj["queue_B"] = queueB;
  
  if (currentMode == MODE_NORMAL) {
    obj["mode"] = "NORMAL";
  } else if (currentMode == MODE_EMERGENCY) {
    obj["mode"] = "EMERGENCY";
  } else {
    obj["mode"] = "FAILSAFE";
  }
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(TOPIC_SIGNAL_STATE, 0, true, output.c_str());
}

// ============================================================================
// MQTT EVENT HANDLERS
// ============================================================================
void onMqttConnect(bool sessionPresent) {
  Serial.println("[MQTT] Connected to broker");
  
  // Publish online status
  StaticJsonDocument<64> doc;
  JsonObject& obj = doc.to<JsonObject>();
  obj["status"] = "online";
  String output;
  serializeJson(doc, output);
  mqttClient.publish(TOPIC_SIGNAL_STATUS, 1, true, output.c_str());
  
  // Subscribe to topics
  mqttClient.subscribe(TOPIC_EMERGENCY_DETECT, 1);
  mqttClient.subscribe(TOPIC_TRAFFIC_A_ARRIVE, 0);
  mqttClient.subscribe(TOPIC_TRAFFIC_B_ARRIVE, 0);
  
  // Resume from failsafe if we were in it
  if (currentMode == MODE_FAILSAFE) {
    resumeFromFailsafe();
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[MQTT] Disconnected, reason: %d\n", (int)reason);
  enterFailsafe();
  
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// ============================================================================
// WIFI EVENT HANDLERS
// ============================================================================
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("[WiFi] Connected!");
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      xTimerStop(wifiReconnectTimer, 0);
      xTimerStart(mqttReconnectTimer, 0);
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WiFi] Disconnected!");
      enterFailsafe();
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;

#ifdef SYSTEM_EVENT_STA_GOT_IP
    // Legacy aliases for older ESP32 core releases
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("[WiFi] Connected!");
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      xTimerStop(wifiReconnectTimer, 0);
      xTimerStart(mqttReconnectTimer, 0);
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("[WiFi] Disconnected!");
      enterFailsafe();
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
#endif

    default:
      break;
  }
}

// ============================================================================
// RECONNECT TIMERS
// ============================================================================
void connectToMqtt() {
  Serial.println("[MQTT] Connecting...");
  mqttClient.connect();
}

void connectToWifi() {
  // Avoid the "wifi:sta is connecting, cannot set config" churn by ensuring we
  // start from a clean STA state before reconnect attempts.
  if (WiFi.isConnected()) {
    Serial.println("[WiFi] Already connected - skipping new attempt");
    return;
  }

  // Warn if the placeholder credentials are still present so the user gets a
  // clear hint about why the station cannot connect.
  if (String(WIFI_SSID) == "YOUR_WIFI_SSID" || String(WIFI_PASSWORD) == "YOUR_WIFI_PASSWORD") {
    Serial.println("[WiFi] ERROR: Update WIFI_SSID and WIFI_PASSWORD in config.h");
  }

  WiFi.persistent(false);      // Do not save credentials to flash
  WiFi.mode(WIFI_STA);         // Ensure station mode
  WiFi.disconnect(true, true); // Drop any half-open connection attempts

  Serial.printf("[WiFi] Connecting to SSID '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("ESP32 Traffic Controller Node - Spec v1.2");
  Serial.println("========================================\n");
  
  // GPIO setup
  pinMode(PIN_A_RED, OUTPUT);
  pinMode(PIN_A_GREEN, OUTPUT);
  pinMode(PIN_B_RED, OUTPUT);
  pinMode(PIN_B_GREEN, OUTPUT);
  
  // Buzzer PWM setup
#if defined(ARDUINO_ESP32_VERSION_MAJOR) && ARDUINO_ESP32_VERSION_MAJOR >= 3
  ledcAttach(PIN_BUZZER, BUZZER_FREQ, BUZZER_RESOLUTION);
#else
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
#endif
  
  // Start in failsafe
  setFailsafeOutputs();
  
  // Create timers
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, 
                                     (void*)0, (TimerCallbackFunction_t)connectToMqtt);
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, 
                                     (void*)0, (TimerCallbackFunction_t)connectToWifi);
  
  // WiFi setup
  WiFi.onEvent(WiFiEvent);
  
  // MQTT setup
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE);
  
  // Set LWT (Last Will and Testament)
  StaticJsonDocument<64> lwtDoc;
  JsonObject& lwtObj = lwtDoc.to<JsonObject>();
  lwtObj["status"] = "offline";
  String lwtPayload;
  serializeJson(lwtDoc, lwtPayload);
  mqttClient.setWill(TOPIC_SIGNAL_STATUS, 1, true, lwtPayload.c_str());
  
  // Connect WiFi
  connectToWifi();
  
  Serial.println("[SETUP] Complete - waiting for connection...\n");
}

// ============================================================================
// MAIN LOOP (NON-BLOCKING)
// ============================================================================
void loop() {
  // Check phase timeout (NORMAL mode)
  checkPhaseTimeout();
  
  // Check anti-starvation (NORMAL mode)
  checkAntiStarvation();
  
  // Check emergency timeout (EMERGENCY mode)
  checkEmergencyTimeout();
  
  // Discharge queues (NORMAL mode only)
  dischargeQueues();
  
  // Publish state
  publishState();
  
  // Small yield to prevent watchdog
  yield();
}
