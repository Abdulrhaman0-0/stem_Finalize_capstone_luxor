# ESP32 Traffic Intersection - Implementation Changelog

## Version 1.0 - Initial Implementation (2026-01-05)

### Created Files

#### 1. Configuration
- **`config.h`** - Shared configuration file
  - WiFi credentials (user must set)
  - MQTT broker settings (broker.emqx.io:1883)
  - All timing parameters per Spec v1.2
  - GPIO pin mappings for both nodes
  - TEAM_ID = "LUXOR"

#### 2. Traffic Controller Firmware
- **`TrafficController/TrafficController.ino`** - Complete controller implementation
  
  **Features Implemented:**
  - ✅ Three-mode state machine: NORMAL, EMERGENCY, FAILSAFE
  - ✅ Two-phase control: A_GREEN (B_RED) and B_GREEN (A_RED)
  - ✅ Snapshot-based adaptive timing
    - Phase duration computed ONCE at start
    - `duration = clamp(MIN_GREEN + queue * EXTEND_STEP, MIN_GREEN, MAX_GREEN)`
  - ✅ Anti-starvation using A_wait_start_ms / B_wait_start_ms
    - Forces switch after MAX_WAIT_MS (30s) for waiting direction
  - ✅ Automatic queue discharge (1 car/sec during NORMAL only)
  - ✅ Emergency preemption
    - Immediate switch to ambulance approach GREEN
    - Buzzer ON for 5 seconds
    - Repeatable (same approach resets timer, different switches)
  - ✅ Failsafe on WiFi/MQTT disconnect
    - All-red state, buzzer OFF
    - Resumes intelligently based on queue sizes
  - ✅ MQTT publishing
    - signal/state every 1000ms (retained)
    - signal/status with LWT (online/offline)
  - ✅ MQTT subscriptions
    - emergency/detect (QoS 1)
    - traffic/A/arrive (QoS 0)
    - traffic/B/arrive (QoS 0)
  - ✅ Non-blocking operation (millis()-based, no delay())
  - ✅ Queue saturation at QUEUE_MAX (50)
  - ✅ PWM buzzer control (GPIO33)
  
  **GPIO Mapping:**
  ```
  A_RED   = GPIO25
  A_GREEN = GPIO26
  B_RED   = GPIO27
  B_GREEN = GPIO14
  BUZZER  = GPIO33
  ```

#### 3. RFID Detector Firmware
- **`RFIDDetector/RFIDDetector.ino`** - Complete RFID node implementation
  
  **Features Implemented:**
  - ✅ RC522 RFID reader (VSPI pins)
  - ✅ 8-second cooldown to prevent tag spam
  - ✅ MQTT emergency publishing (QoS 1)
  - ✅ WiFi reconnection handling
  - ✅ Non-blocking operation
  - ✅ Configurable approach direction
  
  **GPIO Mapping (VSPI - Safe Pins):**
  ```
  SS (SDA) = GPIO21
  SCK      = GPIO18
  MOSI     = GPIO23
  MISO     = GPIO19
  RST      = GPIO22
  ```

#### 4. Documentation
- **`README.md`** - Complete user guide
  - Hardware wiring diagrams
  - Library installation instructions
  - Flashing procedure (Arduino IDE)
  - MQTTX demo script with copy/paste payloads
  - Expected LED behavior tables
  - Serial monitor output examples
  - Troubleshooting guide
  
- **`LIBRARIES.md`** - Arduino library dependencies
  - AsyncMqttClient (MQTT client)
  - ArduinoJson v6.x (JSON parsing)
  - MFRC522 (RFID reader)
  - ESP32 board support installation

### Key Design Decisions

1. **Snapshot Timing (Spec v1.2 Requirement)**
   - Phase duration computed only at phase start
   - Arrivals during current phase do NOT extend it
   - Affects next phase duration instead

2. **No Team ID Filtering (Per User Request)**
   - Controller accepts all messages regardless of team_id field
   - team_id still included in all payloads for multi-team scenarios

3. **Failsafe Mechanism (Spec v1.2)**
   - Triggers on WiFi loss OR MQTT disconnect callback
   - No publish-fail counting (signal/state is QoS 0)

4. **Staleness Detection (Observer-Side)**
   - No NTP time synchronization required
   - Observers track message receipt timestamps locally

5. **GPIO Pin Selection**
   - Primary pinout avoids all strapping pins
   - VSPI used for RC522 (safe pins only)
   - GPIO33 for buzzer (PWM-capable)

### Testing Recommendations

Before deploying for 5-minute demo:

1. **Controller Tests**
   - [ ] WiFi connection successful
   - [ ] MQTT connection + LWT published
   - [ ] signal/state publishes every 1s
   - [ ] Arrivals increment queues
   - [ ] Discharge decrements active queue
   - [ ] Phase switches after timeout
   - [ ] Anti-starvation triggers at 30s
   - [ ] Emergency preempts immediately
   - [ ] Buzzer sounds during emergency
   - [ ] Failsafe on WiFi disconnect

2. **RFID Tests**
   - [ ] Tag detection works
   - [ ] Emergency message published
   - [ ] 8s cooldown enforced
   - [ ] Controller responds to RFID emergency

3. **Integration Tests**
   - [ ] End-to-end: RFID → MQTT → Controller → LED change
   - [ ] Queue discharge during NORMAL only
   - [ ] Emergency pauses discharge
   - [ ] Snapshot timing: arrivals after phase start don't extend current phase

### Known Limitations

1. **No Real-Time Clock**
   - Timestamps in payloads are relative (uptime-based)
   - Acceptable for demo purposes

2. **Public Broker**
   - Topic collisions possible if other teams use same prefix
   - TEAM_ID field helps but not enforced by controller

3. **Single RFID Detector**
   - Template supports one approach
   - Deploy second ESP32 with RFID_APPROACH="B" for full coverage

### File Structure
```
stem_Finalize_capstone_luxor/
├── config.h                          # Root shared config
├── TrafficController/
│   ├── TrafficController.ino         # Controller firmware
│   └── config.h                      # Copy of shared config
├── RFIDDetector/
│   ├── RFIDDetector.ino              # RFID firmware
│   └── config.h                      # Copy of shared config
├── README.md                         # User guide & demo
├── LIBRARIES.md                      # Arduino library deps
└── CHANGELOG.md                      # This file
```

### Next Steps for User

1. Install required Arduino libraries (see LIBRARIES.md)
2. Edit config.h with WiFi credentials
3. Flash TrafficController.ino to first ESP32
4. Flash RFIDDetector.ino to second ESP32
5. Wire LEDs, buzzer, and RC522 per README.md diagrams
6. Open Serial Monitors (115200 baud) on both
7. Use MQTTX to simulate traffic and observe system
8. Present RFID tag to test emergency override
9. Run full demo script from README.md
