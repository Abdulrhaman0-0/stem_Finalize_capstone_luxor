// ============================================================================
// SHARED CONFIGURATION FILE FOR ESP32 TRAFFIC INTERSECTION PROJECT
// Spec v1.2 - Use this in BOTH controller and RFID nodes
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WIFI CREDENTIALS (CHANGE THESE TO YOUR NETWORK)
// ============================================================================
#define WIFI_SSID "YOUR_WIFI_SSID"         // Change this!
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" // Change this!

// ============================================================================
// MQTT BROKER SETTINGS (FIXED - DO NOT CHANGE)
// ============================================================================
#define MQTT_BROKER "broker.emqx.io"
#define MQTT_PORT 1883
#define MQTT_KEEPALIVE 15  // seconds

// ============================================================================
// SESSION ISOLATION (configurable)
// ============================================================================
#define TEAM_ID "LUXOR"

// ============================================================================
// MQTT TOPICS (FIXED - DO NOT CHANGE)
// ============================================================================
#define TOPIC_BASE "stem/Finalize_capstone/"
#define TOPIC_EMERGENCY_DETECT TOPIC_BASE "emergency/detect"
#define TOPIC_TRAFFIC_A_ARRIVE TOPIC_BASE "traffic/A/arrive"
#define TOPIC_TRAFFIC_B_ARRIVE TOPIC_BASE "traffic/B/arrive"
#define TOPIC_SIGNAL_STATE TOPIC_BASE "signal/state"
#define TOPIC_SIGNAL_STATUS TOPIC_BASE "signal/status"

// ============================================================================
// TRAFFIC CONTROLLER TIMING PARAMETERS (ms)
// ============================================================================
#define MIN_GREEN_MS 5000
#define MAX_GREEN_MS 20000
#define EXTEND_STEP_MS 2000
#define MAX_WAIT_MS 30000
#define STATE_PUBLISH_MS 1000
#define DISCHARGE_TICK_MS 1000
#define DISCHARGE_RATE 1
#define QUEUE_MAX 50
#define EMERGENCY_MS 5000

// ============================================================================
// RFID DETECTOR PARAMETERS
// ============================================================================
#define RFID_COOLDOWN_MS 8000
#define RFID_APPROACH "A"  // Change to "B" for second detector

// ============================================================================
// TRAFFIC CONTROLLER GPIO PINS (ESP32 DevKit)
// ============================================================================
#define PIN_A_RED 25
#define PIN_A_GREEN 26
#define PIN_B_RED 27
#define PIN_B_GREEN 14
#define PIN_BUZZER 33

// ============================================================================
// RFID DETECTOR GPIO PINS (ESP32 DevKit + RC522 VSPI)
// ============================================================================
#define PIN_RFID_SS 21
#define PIN_RFID_RST 22
// VSPI standard pins: SCK=18, MOSI=23, MISO=19

// ============================================================================
// BUZZER PWM SETTINGS
// ============================================================================
#define BUZZER_CHANNEL 0
#define BUZZER_FREQ 2000  // 2kHz tone
#define BUZZER_RESOLUTION 8

#endif // CONFIG_H
