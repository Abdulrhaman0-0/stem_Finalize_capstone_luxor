// ============================================================================
// ESP32 RFID AMBULANCE DETECTOR NODE
// Spec v1.2 - Smart Traffic Intersection with MQTT
// ============================================================================

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// ============================================================================
// STATE VARIABLES
// ============================================================================
unsigned long lastPublishTime = 0;
bool mqttConnected = false;

// ============================================================================
// RFID UTILITIES
// ============================================================================
String getTagUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

// ============================================================================
// EMERGENCY PUBLISHER
// ============================================================================
void publishEmergency(String tagUID) {
  if (!mqttConnected) {
    Serial.println("[RFID] Cannot publish - MQTT not connected");
    return;
  }
  
  unsigned long now = millis();
  
  // Cooldown check
  if (now - lastPublishTime < RFID_COOLDOWN_MS) {
    Serial.printf("[RFID] Cooldown active (%lu ms remaining)\n", 
                  RFID_COOLDOWN_MS - (now - lastPublishTime));
    return;
  }
  
  // Build JSON payload
  StaticJsonDocument<128> doc;
  doc["event"] = "ambulance_detected";
  doc["approach"] = RFID_APPROACH;
  doc["tag"] = tagUID;
  doc["team_id"] = TEAM_ID;
  
  String output;
  serializeJson(doc, output);
  
  // Publish
  mqttClient.publish(TOPIC_EMERGENCY_DETECT, 1, false, output.c_str());
  
  Serial.printf("[RFID] EMERGENCY published: %s\n", output.c_str());
  
  lastPublishTime = now;
}

// ============================================================================
// RFID SCANNER
// ============================================================================
void scanRFID() {
  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  
  // Read card serial
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }
  
  String tagUID = getTagUID();
  Serial.printf("[RFID] Tag detected: %s\n", tagUID.c_str());
  
  // Publish emergency
  publishEmergency(tagUID);
  
  // Halt PICC
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ============================================================================
// MQTT EVENT HANDLERS
// ============================================================================
void onMqttConnect(bool sessionPresent) {
  Serial.println("[MQTT] Connected to broker");
  mqttConnected = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[MQTT] Disconnected, reason: %d\n", (int)reason);
  mqttConnected = false;
  
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// ============================================================================
// WIFI EVENT HANDLERS
// ============================================================================
void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("[WiFi] Connected!");
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      xTimerStart(mqttReconnectTimer, 0);
      break;
      
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("[WiFi] Disconnected!");
      mqttConnected = false;
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
      
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
  Serial.println("[WiFi] Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("ESP32 RFID Detector Node - Spec v1.2");
  Serial.printf("Approach: %s\n", RFID_APPROACH);
  Serial.println("========================================\n");
  
  // SPI setup for RC522
  SPI.begin();
  rfid.PCD_Init();
  
  // Show RFID reader details
  rfid.PCD_DumpVersionToSerial();
  Serial.println("\n[RFID] Ready - place tag near reader\n");
  
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
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE);
  
  // Connect WiFi
  connectToWifi();
  
  Serial.println("[SETUP] Complete - waiting for connection...\n");
}

// ============================================================================
// MAIN LOOP (NON-BLOCKING)
// ============================================================================
void loop() {
  scanRFID();
  yield();
}
