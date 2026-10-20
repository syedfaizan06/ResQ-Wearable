# ResQ — Industrial Worker Safety Wearable

<p align="center">
  <strong>Health monitoring · Environmental awareness · Fall detection · Emergency assistance</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-00979D?style=flat-square" alt="Platform: ESP32-S3" />
  <img src="https://img.shields.io/badge/Firmware-Arduino%20C%2B%2B-00599C?style=flat-square" alt="Firmware: Arduino C++" />
  <img src="https://img.shields.io/badge/Connectivity-Bluetooth%20LE-1769AA?style=flat-square" alt="Connectivity: Bluetooth LE" />
  <img src="https://img.shields.io/badge/Prototype-In%20development-orange?style=flat-square" alt="Prototype in development" />
</p>

ResQ is an **ESP32-S3-based industrial wearable prototype** designed to support worker safety in environments such as mines, factories, construction sites, workshops, and other high-risk workplaces. It combines physiological and environmental sensing with motion-based fall-event detection, manual SOS, local alerts, GPS data, and Bluetooth Low Energy (BLE) telemetry.

The goal is to turn a possible incident into useful information for a response: an alert, available sensor context, and location data where GNSS reception is available. The wearable is designed for offline-first use; this repository documents only the features and communication paths currently present in the firmware, with planned system-level capabilities identified separately.

## Prototype

<p align="center">
  <img src="https://github.com/syedfaizan06/ResQ-Wearable/blob/main/images/resq-prototype.jpg?raw=true" alt="ResQ industrial safety wearable prototype mounted on a wrist, showing its OLED dashboard, ESP32-S3 board, GPS antenna, and sensor modules" width="480" />
</p>

*Working bench prototype: ESP32-S3, OLED display, GPS module and sensor breakout boards mounted on a wrist strap for early integration testing.*

> **Prototype note:** This is an engineering prototype, not certified personal protective equipment or a medical device. It must not be used as the sole means of worker protection, emergency response, or medical decision-making.

## Problem

Workers in industrial environments may experience falls, heat exposure, fatigue, or medical distress where the worker's condition and exact location are not immediately known. An alarm alone may not provide responders with enough context to act quickly.

ResQ explores a wearable approach that combines health-related sensor readings, environmental context, motion sensing, local alerting, and wireless telemetry in one compact embedded system.

## Current capabilities

| Capability | Current implementation |
|---|---|
| Motion sensing | MPU6050 accelerometer/gyroscope connected over I²C |
| Possible fall event | Acceleration-magnitude threshold followed by a 10-second grace period |
| Manual SOS | Double-press button input |
| False-alarm cancellation | Hold the button for 2 seconds during the pending fall countdown |
| Local feedback | OLED status/dashboard and buzzer |
| Environmental readings | AHT10 temperature and relative humidity |
| Body-proximate temperature sensor | TMP117 hardware initialisation is present; the current displayed value is simulated, not read from the sensor |
| Optical pulse sensor | MAX30102/MAX3010x-compatible driver initialisation and FIFO processing are present; the currently transmitted HR and SpO₂ values are simulated |
| Outdoor location | NEO-6M GNSS data parsing through UART; GNSS validity must be checked |
| Phone/dashboard link | BLE GATT service with a readable/notifiable telemetry characteristic |
| Risk indicator | Rule-based status currently changes to CRITICAL during a pending fall or active SOS; this is not a trained AI model |

The feature table describes the code as it currently stands, not the full product roadmap. See [Implementation status](#implementation-status) for important caveats.

## Hardware

| Component | Role | Firmware integration |
|---|---|---|
| ESP32-S3 SuperMini | Main controller | Sensor processing, button logic, OLED, buzzer, BLE and GNSS UART |
| MAX30102 | Optical pulse / SpO₂ sensor | MAX3010x library interface; HR/SpO₂ output is currently simulated |
| MPU6050 | Acceleration and angular-rate sensing | Used for acceleration-magnitude threshold detection |
| TMP117 | Temperature sensor placed near the worker's body | Sensor initialisation present; displayed body-temperature value is currently simulated |
| AHT10 | Ambient temperature and humidity | Read over a second I²C bus |
| NEO-6M | GNSS position | UART input parsed with TinyGPSPlus |
| 128 × 64 OLED (SSD1306-compatible) | Local dashboard and event countdown | I²C |
| Push button | Manual SOS / fall cancellation | GPIO input with internal pull-up |
| Buzzer | Local audible alert | GPIO output |
| Battery and power circuitry | Portable operation | Power design and endurance require further validation |

Confirm the exact board and module variants before reproducing the build. The firmware settings below reflect the current sketch, and may need adaptation for other ESP32-S3 boards.

## System overview

```text
 MAX30102       MPU6050       TMP117        AHT10
 HR / SpO2      Motion        Temperature   Ambient T / RH
     \             |             |             /
      \____________|_____________|____________/
                           |
                     ESP32-S3 wearable
                    /       |        \
              Fall/SOS   OLED+buzzer   GNSS (NEO-6M)
                    \       |        /
                      Local event logic
                              |
                        BLE GATT notify
                              |
                    Phone / BLE dashboard

 Future system-level links (not implemented by this sketch):
 wearable <-> site node <-> LoRa <-> central safety hub
```

The current firmware implements the wearable and BLE telemetry path. It does **not** implement the full wearable-to-site-node-to-LoRa-to-central-hub network in this sketch. The industrial multi-node architecture remains a planned integration stage.

## Firmware details

The current wearable firmware uses Arduino C++ and these main libraries:

- **Adafruit GFX** and **Adafruit SSD1306** — OLED rendering.
- **Adafruit MPU6050** and **Adafruit Unified Sensor** — motion sensing.
- **Adafruit AHTX0** — ambient temperature and humidity.
- **Adafruit TMP117** — temperature sensor interface.
- **SparkFun MAX3010x Sensor Library** (`MAX30105.h`, `heartRate.h`) — optical sensor interface and beat detection.
- **TinyGPSPlus** — NMEA/GNSS parsing.
- **ESP32 BLE libraries** — BLE GATT service and notifications.

Install the ESP32 board support and libraries using Arduino IDE's Boards Manager and Library Manager. The current sketch is stored at [`firmware/wearable/resq_wearable.ino`](firmware/wearable/resq_wearable.ino).

### Pin configuration in the current sketch

| Function | GPIO / interface |
|---|---|
| Main I²C SDA | GPIO 8 |
| Main I²C SCL | GPIO 9 |
| AHT10 I²C SDA | GPIO 1 |
| AHT10 I²C SCL | GPIO 2 |
| GNSS UART RX | GPIO 44 |
| GNSS UART TX | GPIO 43 |
| Buzzer | GPIO 7 |
| Button | GPIO 6 |
| OLED I²C address | 0x3C |
| TMP117 I²C address | 0x49 |

The sketch uses `Wire` for the OLED, MPU6050, TMP117, and optical sensor; it uses `Wire1` for the AHT10. Verify that your selected ESP32-S3 board exposes these GPIOs and that module voltage levels are compatible before wiring.

## BLE telemetry

The firmware advertises the BLE device name **`VitalEdge`**. The current GATT service exposes one characteristic with **READ** and **NOTIFY** properties.

When a BLE client is connected, telemetry is sent approximately every two seconds as a comma-separated record with this field order:

```text
HR,SpO2,BodyTemp,EnvTemp,Humidity,Risk,RiskScore,SOS,Latitude,Longitude
```

Example format only (not measured output):

```text
72,98,34.0,28.6,71,NORMAL,10,0,23.4242,85.4326
```

The current sketch uses fallback coordinates if GNSS has no valid location fix. Those fallback coordinates are not proof of the wearer's actual location. A consuming dashboard should treat the location as valid only when GNSS validity is explicitly confirmed; the current CSV payload does not include a dedicated GNSS-validity field.

## Emergency logic

### Manual SOS
1. Press the button twice within the configured 500 ms double-press window.
2. The firmware sets the SOS state and raises the risk score.
3. It requests an immediate BLE telemetry send, displays a status message, and activates the buzzer for five seconds.

The immediate telemetry send requires an active BLE connection; without one, local feedback still occurs but the current sketch does not relay the event to a remote site hub.

### Possible fall
1. The MPU6050 acceleration magnitude is sampled at approximately 20 Hz.
2. If the measured magnitude exceeds the configured 2.5 g threshold, a fall-pending countdown begins.
3. The OLED shows a 10-second countdown.
4. Holding the button for two seconds during this window cancels the pending event.
5. If the countdown expires, the sketch activates the fall SOS state and local buzzer.

This is a basic threshold-based prototype algorithm. It has not been demonstrated to deliver a particular fall-detection accuracy and may produce false positives or miss falls. Validation across different movements, worker activities, mounting positions and protective clothing is required.

## Implementation status

**Implemented in the current sketch**
- Initialisation attempts for the OLED and listed sensors.
- AHT10 environmental temperature and humidity acquisition.
- MPU6050 acceleration-magnitude threshold logic.
- Double-press manual SOS and fall-event cancellation window.
- OLED dashboard, event message, and timed buzzer.
- BLE advertising and periodic CSV telemetry.
- GNSS serial parsing.

**Present but not yet providing validated sensor output**
- HR and SpO₂ are currently assigned simulated values in the code.
- The displayed body-proximate temperature is currently generated as a simulated value; the TMP117 read is bypassed.
- Risk scoring is a simple rule-based indicator with SOS/fall overrides, not TinyML inference or validated health-risk classification.

**Planned / not implemented in this sketch**
- End-to-end LoRa communication between wearable, site node and central safety hub.
- Industrial central monitoring dashboard and multi-worker coordination.
- Validated heat-stress or fatigue inference using sensor fusion.
- TinyML-based on-device anomaly classification.
- Production PCB, enclosure, and measured battery endurance.
- Directional responder guidance using positions and orientation.

## Build and upload

1. Install the Arduino IDE and ESP32 board support.
2. Install the libraries listed in [Firmware details](#firmware-details).
3. Open [`firmware/wearable/resq_wearable.ino`](firmware/wearable/resq_wearable.ino).
4. Select the matching ESP32-S3 board and serial port.
5. Check GPIO assignments and sensor wiring against [Pin configuration](#pin-configuration-in-the-current-sketch).
6. Compile and upload the sketch.
7. Open Serial Monitor at **115200 baud** to inspect initialisation messages and BLE telemetry logs.
8. Connect a BLE client that supports notifications to the advertised `VitalEdge` device and subscribe to the characteristic.

The current firmware has not yet been compiled in a clean environment from this repository, so verify the exact library and board package versions before relying on this build guide.

## Safety and data considerations

- This prototype is for engineering demonstration and research. It is not certified PPE and does not replace site safety procedures, supervisors, or emergency response systems.
- Optical HR/SpO₂ modules and skin-contact temperature readings are sensitive to placement, movement, environment and sensor quality. They are not clinical measurements by default.
- A general-purpose prototype must not claim that its readings diagnose heat illness, fatigue, or a medical condition.
- GNSS may be unavailable or inaccurate indoors, underground, or around obstructed structures.
- BLE telemetry currently includes health-related fields and coordinates. Do not transmit real worker data without consent, appropriate access controls, and a clear retention policy. Do not commit private worker data or credentials.

## Roadmap

- Add a location-validity flag and robust reconnect/error handling.
- Improve fall-event classification and validate performance with controlled testing.
- Implement and test the site-node and LoRa communication path.
- Integrate a monitoring dashboard for authorised site personnel.
- Measure power consumption and redesign the wearable PCB and enclosure.
- Explore TinyML only after collecting representative, labelled data and establishing a reproducible evaluation process.

## Team and project context

ResQ is being developed as an industrial worker safety and health-monitoring wearable. The project was presented by **Team Vital Edge** in the Smart India Hackathon 2026 context.

## License

No open-source license has been selected for this repository yet. Until a license is added, do not assume that the code is licensed for reuse or redistribution.
