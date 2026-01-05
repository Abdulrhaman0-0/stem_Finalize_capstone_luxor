# ESP32 Smart Traffic Intersection - MQTT Capstone Project
**Specification Version:** 1.2  
**Team ID:** LUXOR

## Project Overview
This project implements a 2-way adaptive traffic intersection controller using two ESP32 microcontrollers communicating via MQTT. It features:
- Real-time adaptive traffic signal control
- Emergency vehicle (ambulance) RFID detection and preemption
- Vehicle arrival simulation via MQTT
- Automatic queue discharge
- Failsafe operation on connectivity loss

---

## Hardware Requirements

### Traffic Controller Node
| Component | Specification |
|-----------|---------------|
| Microcontroller | ESP32 DevKit (WROOM-32) |
| LEDs | 2× Red + 2× Green (standard 5mm) |
| Resistors | 2× 270Ω (red LEDs) + 2× 150Ω (green LEDs) |
| Buzzer | Piezo buzzer (<15mA) or magnetic (>20mA with transistor) |

### RFID Detector Node
| Component | Specification |
|-----------|---------------|
| Microcontroller | ESP32 DevKit (WROOM-32) |
| RFID Reader | RC522 module (13.56MHz) |
| Power | **3.3V ONLY** (NOT 5V!) |

---

## Wiring Diagrams

### Traffic Controller Node Pinout
```
ESP32 GPIO    Component
──────────────────────────────
GPIO 25   →   A_RED LED (270Ω → GND)
GPIO 26   →   A_GREEN LED (150Ω → GND)
GPIO 27   →   B_RED LED (270Ω → GND)
GPIO 14   →   B_GREEN LED (150Ω → GND)
GPIO 33   →   Buzzer (→ GND for piezo)

3V3       →   +3.3V rail
GND       →   Ground rail
```

### RFID Detector Node Pinout (VSPI)
```
RC522 Pin     ESP32 GPIO     Notes
──────────────────────────────────────────
SDA (SS)  →   GPIO 21        Chip Select
SCK       →   GPIO 18        VSPI Clock
MOSI      →   GPIO 23        VSPI Data Out
MISO      →   GPIO 19        VSPI Data In
RST       →   GPIO 22        Reset
3.3V      →   3V3            ⚠️ 3.3V ONLY!
GND       →   GND            Ground
IRQ       →   NC             Not used
```

> **⚠️ CRITICAL:** RC522 operates at 3.3V only. Connecting VCC to 5V will permanently damage the module!

---

## Software Setup

### Required Libraries
Install these libraries via Arduino IDE Library Manager:

1. **AsyncMqttClient** (by Marvin Roger)
2. **ArduinoJson** (by Benoit Blanchon) - version 6.x
3. **MFRC522** (by GithubCommunity)

Also ensure you have the **ESP32 board support** installed:
- In Arduino IDE: File → Preferences → Additional Board Manager URLs
- Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Tools → Board → Boards Manager → Search "ESP32" → Install

### Configuration
1. Open `config.h` in both project folders
2. **Set your WiFi credentials:**
   ```cpp
   #define WIFI_SSID "Your_Network_Name"
   #define WIFI_PASSWORD "Your_Password"
   ```
3. **Verify TEAM_ID** (default is "LUXOR")
4. **For second RFID node** (if deploying two approaches):
   - Change `#define RFID_APPROACH "A"` to `"B"`

### Flashing Instructions

#### Traffic Controller
1. Open `TrafficController/TrafficController.ino` in Arduino IDE
2. Tools → Board → ESP32 Dev Module
3. Tools → Port → Select your ESP32's COM port
4. Click **Upload** (→)
5. Open Serial Monitor (115200 baud) to view logs

#### RFID Detector
1. Open `RFIDDetector/RFIDDetector.ino` in Arduino IDE
2. Tools → Board → ESP32 Dev Module
3. Tools → Port → Select your ESP32's COM port
4. Click **Upload** (→)
5. Open Serial Monitor (115200 baud) to view logs

---

## MQTT Topics Reference

| Topic | Direction | QoS | Retained | Purpose |
|-------|-----------|-----|----------|---------|
| `stem/Finalize_capstone/emergency/detect` | RFID → Controller | 1 | No | Emergency event |
| `stem/Finalize_capstone/traffic/A/arrive` | Sim → Controller | 0 | No | Car arrival A |
| `stem/Finalize_capstone/traffic/B/arrive` | Sim → Controller | 0 | No | Car arrival B |
| `stem/Finalize_capstone/signal/state` | Controller → All | 0 | Yes | Controller state |
| `stem/Finalize_capstone/signal/status` | Controller → All | 1 | Yes | Online/offline (LWT) |

---

## Demo Procedure with MQTTX

### 1. Setup MQTTX
- Download from: https://mqttx.app/
- Create new connection:
  - **Name:** Traffic Demo
  - **Host:** broker.emqx.io
  - **Port:** 1883
  - **Client ID:** `stem_sim_LUXOR_yourname`

### 2. Subscribe to Topics
Subscribe to observe system state:
```
stem/Finalize_capstone/signal/state
stem/Finalize_capstone/signal/status
```

### 3. MQTTX Payload Snippets (Copy/Paste)

#### Simulate Car Arrival - Direction A
```json
Topic: stem/Finalize_capstone/traffic/A/arrive
QoS: 0
Payload:
{
  "car_id": "CAR_A1",
  "team_id": "LUXOR"
}
```

#### Simulate Car Arrival - Direction B
```json
Topic: stem/Finalize_capstone/traffic/B/arrive
QoS: 0
Payload:
{
  "car_id": "CAR_B1",
  "team_id": "LUXOR"
}
```

#### Manual Emergency Trigger (for testing without RFID)
```json
Topic: stem/Finalize_capstone/emergency/detect
QoS: 1
Payload:
{
  "event": "ambulance_detected",
  "approach": "A",
  "tag": "TEST1234",
  "team_id": "LUXOR"
}
```

---

## Demo Script

### Phase 1: System Startup (2 min)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Power on Traffic Controller | Serial: "WiFi Connected", "MQTT Connected" |
| 2 | Check MQTTX | `signal/status` = `{"status":"online","team_id":"LUXOR"}` |
| 3 | Observe LEDs | A_GREEN on, B_RED on (default start) |
| 4 | Observe MQTTX | `signal/state` updating every 1 second |

### Phase 2: Normal Traffic Operation (5 min)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 5 | Publish 5 arrivals to A | `queue_A` increases to 5 |
| 6 | Watch phase duration | A_GREEN duration extends (5s + 5×2s = 15s) |
| 7 | Observe discharge | `queue_A` decreases by 1 every second |
| 8 | Wait for phase end | Switches to B_GREEN |
| 9 | Publish 8 arrivals to B | `queue_B` = 8, B_GREEN extends to 20s (MAX_GREEN) |
| 10 | Observe discharge | `queue_B` decreases by 1/sec during B_GREEN |

### Phase 3: Anti-Starvation Demo (3 min)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 11 | During A_GREEN, send continuous A arrivals | `queue_A` stays high |
| 12 | Send 1 arrival to B | `queue_B` = 1 |
| 13 | Wait 35 seconds | After 30s B wait, forces switch to B_GREEN (anti-starvation) |

### Phase 4: Emergency Override (RFID) (3 min)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 14 | During any phase, present RFID tag | Serial: "Tag detected: [UID]" |
| 15 | Check controller | Immediate switch to approach GREEN, buzzer ON |
| 16 | Check MQTTX | `mode` = "EMERGENCY" in signal/state |
| 17 | Wait 5 seconds | Buzzer OFF, mode returns to "NORMAL" |
| 18 | Try tag again immediately | No publish (8s cooldown active) |
| 19 | Wait 8+ seconds, retry | Emergency triggers again |

### Phase 5: Manual Emergency (without RFID) (2 min)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 20 | Publish emergency/detect (approach "B") | Immediate B_GREEN + buzzer |
| 21 | After 3s, publish emergency (approach "A") | Switches to A_GREEN + buzzer (timer reset) |
| 22 | Wait 5s | Buzzer OFF, stays at A_GREEN in NORMAL mode |

---

## Expected LED Behavior

### Normal Operation
| Phase | A_RED | A_GREEN | B_RED | B_GREEN | Buzzer |
|-------|-------|---------|-------|---------|--------|
| A_GREEN | OFF | **ON** | **ON** | OFF | OFF |
| B_GREEN | **ON** | OFF | OFF | **ON** | OFF |

### Emergency Mode
| Mode | Active Approach | Buzzer | Duration |
|------|-----------------|--------|----------|
| EMERGENCY (A) | A_GREEN, B_RED | **ON** | 5 seconds |
| EMERGENCY (B) | A_RED, B_GREEN | **ON** | 5 seconds |

### Failsafe Mode
| Mode | All LEDs | Buzzer | Trigger |
|------|----------|--------|---------|
| FAILSAFE | A_RED + B_RED | OFF | WiFi/MQTT disconnect |

---

## Serial Monitor Output Examples

### Normal Operation
```
[PHASE] A_GREEN started, duration=7000ms, queue snapshot: A=1 B=0
[DISCHARGE] A: 0 remaining
[PHASE] B_GREEN started, duration=5000ms, queue snapshot: A=0 B=0
```

### Emergency Event
```
[RFID] Tag detected: A1B2C3D4
[RFID] EMERGENCY published: {"event":"ambulance_detected","approach":"A","tag":"A1B2C3D4","team_id":"LUXOR"}
[EMERGENCY] Preemption for approach A
[EMERGENCY] Timeout - returning to NORMAL
```

### Anti-Starvation
```
[ANTI-STARVATION] Forcing switch to B_GREEN
```

---

## Timing Parameters (Configurable in config.h)

| Parameter | Default | Description |
|-----------|---------|-------------|
| MIN_GREEN_MS | 5000 | Minimum green phase duration |
| MAX_GREEN_MS | 20000 | Maximum green phase duration |
| EXTEND_STEP_MS | 2000 | Extension per queued vehicle |
| MAX_WAIT_MS | 30000 | Anti-starvation threshold (30s) |
| EMERGENCY_MS | 5000 | Emergency override duration |
| RFID_COOLDOWN_MS | 8000 | RFID tag debounce time |
| QUEUE_MAX | 50 | Maximum queue size |

---

## Troubleshooting

### Controller Issues

| Problem | Solution |
|---------|----------|
| LEDs don't light | Check resistor values and polarity |
| Buzzer doesn't sound | Verify GPIO33 connection; try direct connection for piezo |
| WiFi won't connect | Check SSID/password in config.h |
| MQTT won't connect | Verify broker.emqx.io is reachable; check firewall |
| All-red state persists | Check serial log for disconnect reason |

### RFID Issues

| Problem | Solution |
|---------|----------|
| Tags not detected | Power cycle; verify 3.3V power; check SPI wiring |
| Module damaged | Verify you didn't connect VCC to 5V (fatal!) |
| Cooldown too long | Adjust RFID_COOLDOWN_MS in config.h |

---

## Project Structure
```
stem_Finalize_capstone_luxor/
├── config.h                          # Shared configuration
├── TrafficController/
│   └── TrafficController.ino         # Controller firmware
├── RFIDDetector/
│   └── RFIDDetector.ino              # RFID node firmware
└── README.md                         # This file
```

---

## License & Credits
- **Project:** ESP32 Smart Traffic Intersection Capstone
- **Specification:** v1.2
- **Team:** LUXOR
- **MQTT Broker:** broker.emqx.io (public)

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────┐
│ QUICK REFERENCE                                                 │
├─────────────────────────────────────────────────────────────────┤
│ BROKER:  broker.emqx.io:1883                                   │
│ TEAM_ID: "LUXOR"                                               │
│ PREFIX:  stem/Finalize_capstone/                               │
├─────────────────────────────────────────────────────────────────┤
│ CONTROLLER GPIO:                                                │
│   A_RED=25  A_GREEN=26  B_RED=27  B_GREEN=14  BUZZER=33       │
├─────────────────────────────────────────────────────────────────┤
│ RFID GPIO (VSPI):                                              │
│   SS=21  SCK=18  MOSI=23  MISO=19  RST=22  [3.3V ONLY!]       │
├─────────────────────────────────────────────────────────────────┤
│ TIMING:                                                         │
│   MIN_GREEN=5s   MAX_GREEN=20s   EMERGENCY=5s                  │
│   DISCHARGE=1/s  COOLDOWN=8s     MAX_WAIT=30s                  │
└─────────────────────────────────────────────────────────────────┘
```
