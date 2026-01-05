# ESP32 Traffic Intersection - Required Arduino Libraries

## Installation Instructions
1. Open Arduino IDE
2. Go to: Sketch → Include Library → Manage Libraries
3. Search for each library below and click "Install"

## Required Libraries

### 1. AsyncMqttClient
- **Author:** Marvin Roger
- **Version:** Latest (1.0.1 or higher)
- **Purpose:** Asynchronous MQTT client for ESP32
- **Search term:** "AsyncMqttClient"

### 2. ArduinoJson
- **Author:** Benoit Blanchon
- **Version:** 6.x (NOT 5.x)
- **Purpose:** JSON parsing and serialization
- **Search term:** "ArduinoJson"
- **⚠️ Important:** Make sure to install version 6.x

### 3. MFRC522
- **Author:** GithubCommunity
- **Version:** Latest (1.4.x or higher)
- **Purpose:** RC522 RFID reader support
- **Search term:** "MFRC522"

### 4. ESP32 Board Support
If not already installed:
1. File → Preferences
2. Additional Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Tools → Board → Boards Manager
4. Search "ESP32" and install "esp32 by Espressif Systems"

## Verification
After installing all libraries, you should be able to compile both sketches:
- `TrafficController/TrafficController.ino`
- `RFIDDetector/RFIDDetector.ino`

## Board Settings (for both sketches)
- **Board:** ESP32 Dev Module
- **Upload Speed:** 115200
- **CPU Frequency:** 240MHz (WiFi/BT)
- **Flash Frequency:** 80MHz
- **Flash Mode:** QIO
- **Flash Size:** 4MB (32Mb)
- **Partition Scheme:** Default 4MB with spiffs
- **Core Debug Level:** None (or "Info" for detailed logs)
- **PSRAM:** Disabled

## Troubleshooting

### AsyncMqttClient compilation errors
If you see errors related to `AsyncMqttClient`, you may also need:
- **ESPAsyncTCP** (for ESP8266, not needed for ESP32)
- **AsyncTCP** (should auto-install with ESP32 board support)

### ArduinoJson version conflict
- Uninstall version 5.x if installed
- Install latest 6.x version
- The API changed significantly between v5 and v6

### MFRC522 not found
- Ensure library name is exactly "MFRC522" (by GithubCommunity)
- Alternative: Search for "rfid" in Library Manager
