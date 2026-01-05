# ESP32 Traffic Intersection - Pre-Deployment Validation Checklist

## ✅ Completed Implementation

### Files Created
- [x] `config.h` - Shared configuration (WiFi, MQTT, timing, pins)
- [x] `TrafficController/TrafficController.ino` - Controller firmware (642 lines)
- [x] `RFIDDetector/RFIDDetector.ino` - RFID detector firmware (173 lines)
- [x] `README.md` - Complete user guide with demo procedure
- [x] `LIBRARIES.md` - Arduino library dependencies
- [x] `CHANGELOG.md` - Implementation notes

### Spec v1.2 Compliance

#### MQTT Topics (All Implemented)
- [x] `stem/Finalize_capstone/emergency/detect` (QoS 1)
- [x] `stem/Finalize_capstone/traffic/A/arrive` (QoS 0)
- [x] `stem/Finalize_capstone/traffic/B/arrive` (QoS 0)
- [x] `stem/Finalize_capstone/signal/state` (QoS 0, retained)
- [x] `stem/Finalize_capstone/signal/status` (QoS 1, retained, LWT)

#### JSON Payloads (All Implemented)
- [x] Emergency: `{"event":"ambulance_detected","approach":"A","tag":"...","team_id":"LUXOR"}`
- [x] Arrive: `{"car_id":"...","team_id":"LUXOR"}`
- [x] State: `{"phase":"A_GREEN","remaining_ms":12000,"queue_A":7,"queue_B":3,"mode":"NORMAL","team_id":"LUXOR"}`
- [x] Status: `{"status":"online","team_id":"LUXOR"}` + LWT offline

#### Timing Parameters (All Implemented)
- [x] MIN_GREEN_MS = 5000
- [x] MAX_GREEN_MS = 20000
- [x] EXTEND_STEP_MS = 2000
- [x] MAX_WAIT_MS = 30000
- [x] STATE_PUBLISH_MS = 1000
- [x] DISCHARGE_TICK_MS = 1000
- [x] DISCHARGE_RATE = 1
- [x] QUEUE_MAX = 50
- [x] EMERGENCY_MS = 5000
- [x] RFID_COOLDOWN_MS = 8000

#### Traffic Logic (All Implemented)
- [x] **Snapshot timing**: Phase duration computed ONCE at start
- [x] **Two phases only**: A_GREEN (B_RED) + B_GREEN (A_RED)
- [x] **Anti-starvation**: A_wait_start_ms / B_wait_start_ms tracking
- [x] **Automatic discharge**: 1 car/sec during NORMAL only
- [x] **Emergency preemption**: Immediate switch + buzzer for 5s
- [x] **Repeated emergencies**: Same approach resets timer, different switches
- [x] **Failsafe**: WiFi/MQTT disconnect → all-red
- [x] **Resume from failsafe**: Choose phase by queue size (tie: alternate)
- [x] **Queue saturation**: Cap at QUEUE_MAX with warning log
- [x] **Non-blocking**: millis()-based, no delay() calls

#### Hardware Pinout (All Implemented)
- [x] Traffic Controller GPIO:
  - A_RED = GPIO25
  - A_GREEN = GPIO26
  - B_RED = GPIO27
  - B_GREEN = GPIO14
  - BUZZER = GPIO33 (PWM)
- [x] RFID Detector GPIO (VSPI safe pins):
  - SS = GPIO21
  - SCK = GPIO18
  - MOSI = GPIO23
  - MISO = GPIO19
  - RST = GPIO22

#### MQTT Broker Settings (All Implemented)
- [x] Host: broker.emqx.io
- [x] Port: 1883
- [x] Keep-alive: 15 seconds
- [x] LWT configured on signal/status

---

## 📋 Pre-Flash Checklist

### User Must Complete
- [ ] Install Arduino libraries (see `LIBRARIES.md`)
  - [ ] AsyncMqttClient
  - [ ] ArduinoJson v6.x
  - [ ] MFRC522
  - [ ] ESP32 board support
- [ ] Edit `config.h` with WiFi credentials
  - [ ] Set WIFI_SSID
  - [ ] Set WIFI_PASSWORD
- [ ] Verify TEAM_ID setting (default: "LUXOR")

### Hardware Assembly
- [ ] Wire Traffic Controller LEDs with correct resistors
  - [ ] A_RED → GPIO25 + 270Ω
  - [ ] A_GREEN → GPIO26 + 150Ω
  - [ ] B_RED → GPIO27 + 270Ω
  - [ ] B_GREEN → GPIO14 + 150Ω
- [ ] Wire buzzer → GPIO33
- [ ] Wire RC522 to RFID node (VSPI pins)
  - [ ] **Verify 3.3V power only!**
  - [ ] SS → GPIO21
  - [ ] SCK → GPIO18
  - [ ] MOSI → GPIO23
  - [ ] MISO → GPIO19
  - [ ] RST → GPIO22

### Flashing
- [ ] Flash `TrafficController.ino` to first ESP32
- [ ] Flash `RFIDDetector.ino` to second ESP32
- [ ] Open Serial Monitor (115200 baud) for both
- [ ] Verify WiFi connection logs
- [ ] Verify MQTT connection logs

---

## 🧪 Validation Tests

### Test 1: Controller Startup
**Expected:**
- Serial: "WiFi Connected"
- Serial: "MQTT Connected"
- LEDs: A_GREEN on, B_RED on
- MQTTX: signal/status = "online"
- MQTTX: signal/state updating every 1s

### Test 2: Vehicle Arrivals
**Steps:**
1. Publish 3× arrivals to A via MQTTX
**Expected:**
- queue_A increases to 3
- Current phase duration doesn't change (snapshot timing)
- Next A_GREEN phase = 5s + 3×2s = 11s

### Test 3: Discharge
**Expected:**
- During A_GREEN: queue_A decreases by 1 every second
- During B_GREEN: queue_B decreases by 1 every second
- During EMERGENCY: no discharge

### Test 4: Anti-Starvation
**Steps:**
1. Send continuous arrivals to A
2. Send 1 arrival to B
3. Wait 35 seconds
**Expected:**
- Forces switch to B_GREEN after 30s B wait time

### Test 5: RFID Emergency
**Steps:**
1. Present RFID tag during any phase
**Expected:**
- Serial: "Tag detected: [UID]"
- Immediate switch to approach GREEN
- Buzzer ON
- mode="EMERGENCY" in signal/state
- After 5s: buzzer OFF, mode="NORMAL"

### Test 6: RFID Cooldown
**Steps:**
1. Present tag
2. Keep tag in range
**Expected:**
- Only 1 publish
- Second detection ignored for 8 seconds

### Test 7: Manual Emergency (MQTTX)
**Steps:**
1. Publish emergency/detect with approach="A"
**Expected:**
- Same behavior as RFID emergency

### Test 8: Failsafe
**Steps:**
1. Disconnect WiFi
**Expected:**
- All-red state (A_RED + B_RED both on)
- Buzzer OFF
- mode="FAILSAFE"
- On reconnect: resumes NORMAL

---

## 🚀 5-Minute Demo Readiness

### Pre-Demo
- [ ] Both ESP32s powered and connected
- [ ] Serial monitors open and showing logs
- [ ] MQTTX connected and subscribed to signal/state, signal/status
- [ ] RFID tag ready
- [ ] All LEDs working
- [ ] Buzzer working

### Demo Flow
1. **Startup** (30s): Show online status, A_GREEN default
2. **Normal Traffic** (2 min): Send arrivals, show adaptive timing and discharge
3. **Anti-Starvation** (1 min): Demonstrate fairness guarantee
4. **RFID Emergency** (1 min): Show immediate preemption + buzzer
5. **Manual Emergency** (30s): MQTTX emergency trigger

### Success Criteria
- ✅ No watchdog resets or crashes
- ✅ State publishes continuously every 1s
- ✅ Emergency < 1s end-to-end latency
- ✅ Anti-starvation triggers at 30s exactly
- ✅ Buzzer sounds only during emergency

---

## 🔍 Known Issues & Limitations

### None Currently Known
All Spec v1.2 requirements implemented. No team_id filtering per user request.

### Future Enhancements (Out of Scope)
- NTP time synchronization for accurate timestamps
- Multiple RFID tags support
- Web dashboard for monitoring
- EEPROM persistence of queue state across reboots

---

## 📞 Support

If compilation fails:
1. Check LIBRARIES.md for correct library versions
2. Ensure ESP32 board support is installed
3. Verify Arduino IDE is set to "ESP32 Dev Module"

If runtime fails:
1. Check Serial Monitor for error messages
2. Verify WiFi credentials in config.h
3. Test MQTT broker reachability (ping broker.emqx.io)
4. Check wiring against README.md diagrams
