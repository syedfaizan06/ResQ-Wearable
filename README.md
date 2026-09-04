# VitalEdge
# ESP32 Wearable Health Monitor

An ESP32-based wearable monitoring prototype that integrates multiple sensors to collect physiological and motion-related data.

## Features

* Heart-rate monitoring using MAX30100
* Motion/orientation sensing using MPU6050
* GPS location tracking using NEO-6M
* OLED display for real-time information
* ESP32 as the main controller

## Hardware Used

| Component    |    Quantity |
| ------------ | ----------: |
| ESP32        |           1 |
| MAX30100     |           1 |
| MPU6050      |           1 |
| NEO-6M GPS   |           1 |
| OLED Display |           1 |
| Jumper wires | As required |
| Breadboard   |           1 |

## Circuit

See the complete circuit diagram in the `circuit` folder.

## Pin Connections

### I2C Sensors

The MAX30100, MPU6050 and OLED communicate with the ESP32 using I2C.

| ESP32   | I2C |
| ------- | --- |
| GPIO 21 | SDA |
| GPIO 22 | SCL |

### GPS

| NEO-6M | ESP32                 |
| ------ | --------------------- |
| TX     | RX                    |
| RX     | TX                    |
| VCC    | 5V/appropriate supply |
| GND    | GND                   |

> Check the voltage requirements of your specific module before connecting it.

## Software Requirements

* Arduino IDE
* ESP32 board package
* Required sensor libraries

Install the required libraries through:

**Arduino IDE → Library Manager**

## Uploading the Code

1. Install Arduino IDE.
2. Install ESP32 board support.
3. Install the required libraries.
4. Open `src/wearable_monitor.ino`.
5. Select the correct ESP32 board.
6. Select the correct COM port.
7. Compile the program.
8. Upload it to the ESP32.

## Project Structure

```text
ESP32-Wearable-Monitor/
├── README.md
├── src/
│   └── wearable_monitor.ino
├── circuit/
│   └── circuit_diagram.png
├── images/
│   ├── prototype.jpg
│   └── wiring.jpg
└── docs/
    └── project_report.pdf
```

## Current Status

Prototype under development.

## Known Limitations

* Sensor readings may require filtering and calibration.
* Heart-rate measurements can be affected by finger/wrist movement.
* GPS requires adequate satellite reception.
* The prototype is not intended for medical diagnosis.

## Future Improvements

* Improve sensor filtering and signal processing.
* Reduce the size of the hardware.
* Design a custom PCB.
* Add wireless data transmission.
* Develop a mobile/web dashboard.
* Improve battery management.

## License

This project is provided for educational and research purposes.
