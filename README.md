# ResQ — Industrial Worker Safety Wearable

<p align="center">
  <strong>Worker health · Environmental monitoring · Fall detection · Emergency alerts</strong><br>
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-00979D?style=flat-square" alt="ESP32-S3">
  <img src="https://img.shields.io/badge/Firmware-Arduino%20C%2B%2B-00599C?style=flat-square" alt="Arduino C++">
  <img src="https://img.shields.io/badge/Connectivity-BLE-1769AA?style=flat-square" alt="Bluetooth Low Energy">
  <img src="https://img.shields.io/badge/Status-Prototype-orange?style=flat-square" alt="Prototype">
</p>

ResQ is an **ESP32-S3 wearable prototype for industrial worker safety**, intended for environments such as mines, factories, construction sites, and workshops. It combines environmental sensing, motion-based fall-event detection, manual SOS, local alerts, GNSS data, and Bluetooth Low Energy (BLE) telemetry.

The aim is to give responders useful incident context rather than only an alarm. The current firmware implements local alerts and a BLE telemetry link; the complete site-wide communication system is still under development.

## Prototype

<p align="center">
  <img src="https://github.com/syedfaizan06/ResQ-Wearable/blob/main/images/resq-prototype.jpg?raw=true" alt="ResQ wearable prototype with OLED, ESP32-S3, GPS and sensor modules" width="480">
</p>

*Working prototype mounted on a wrist strap for early hardware integration and testing.*

> **Safety note:** ResQ is an engineering prototype, not certified protective equipment or a medical device. It does not replace workplace safety procedures or emergency response systems.

## Key features

- **Environmental monitoring:** AHT10 measures ambient temperature and relative humidity.
- **Motion-based event detection:** MPU6050 acceleration magnitude is checked against a threshold.
- **Manual SOS:** Double-press the button to trigger a local alert.
- **Fall-event cancellation:** Hold the button for two seconds during the 10-second countdown.
- **Local feedback:** OLED dashboard and buzzer.
- **Location data:** NEO-6M GNSS parsing for outdoor positioning when a valid fix is available.
- **BLE telemetry:** Sends a comma-separated data record to a connected phone or dashboard.

### Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 SuperMini | Main controller and BLE |
| MAX30102 | Optical pulse / SpO₂ sensor module |
| MPU6050 | Motion sensing |
| TMP117 | Body-proximate temperature sensing |
| AHT10 | Ambient temperature and humidity |
| NEO-6M | GNSS location |
| SSD1306-compatible 128 × 64 OLED | Local status and event countdown |
| Push button and buzzer | SOS, cancellation, and local alert |


## Firmware and setup

Firmware: [`firmware/wearable/resq_wearable.ino`](firmware/wearable/resq_wearable.ino)

The sketch uses Arduino C++ and these libraries:

- Adafruit GFX and SSD1306 — OLED display
- Adafruit MPU6050 and Adafruit Unified Sensor — motion sensing
- Adafruit AHTX0 and Adafruit TMP117 — environmental and temperature sensors
- SparkFun MAX3010x Sensor Library — optical sensor interface
- TinyGPSPlus — GNSS parsing
- ESP32 BLE libraries — GATT service and notifications

To build, install the ESP32 board support and libraries in Arduino IDE, open the sketch above, select the matching ESP32-S3 board and port, then compile and upload. Use Serial Monitor at **115200 baud** for logs. A BLE client must connect to the device advertised as `VitalEdge` and subscribe to its notify characteristic.


## Emergency logic

- **Manual SOS:** two button presses within 500 ms activate the SOS state and the buzzer for five seconds. If BLE is connected, the firmware attempts to send telemetry immediately.
- **Possible fall:** the MPU6050 is sampled at approximately 20 Hz. An acceleration magnitude above 2.5 g starts a 10-second countdown. Holding the button for two seconds cancels the pending event; otherwise, a fall SOS and local buzzer are triggered.

This is a basic threshold-based algorithm and has not been validated for industrial fall detection. False alarms and missed events are possible.


## Next steps

- Designing a compact PCB with all sensors integrated into it.
- Add explicit GNSS-validity reporting and robust BLE reconnect handling.
- Test fall-event behaviour and measure false positives, missed events, and alert latency.
- Implement the site-node and LoRa communication path and central monitoring interface.
- Measure battery life and refine the PCB and enclosure.
- Evaluate TinyML only after collecting representative, labelled data.
