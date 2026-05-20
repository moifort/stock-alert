# stock-alert

Matter-over-Wi-Fi stock level sensor for Apple HomeKit. A XIAO ESP32-S3 reads a VL53L1X
time-of-flight sensor mounted above a container and exposes a Matter ContactSensor that
"opens" when the stock falls below a configurable threshold.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-S3 Sense (ESP32-S3, 8MB flash, 8MB PSRAM, native USB)
- **Sensor:** VL53L1X ToF (I2C, range 4cm–400cm)
- **Matter controller:** Apple HomePod mini (or Apple TV / HomePod 2nd gen)

### Wiring

| VL53L1X pin | XIAO ESP32-S3 pin |
| ----------- | ----------------- |
| `VCC`       | `3V3`             |
| `GND`       | `GND`             |
| `SDA`       | `D4` (GPIO5)      |
| `SCL`       | `D5` (GPIO6)      |

> Never feed 5V to the sensor — the XIAO ESP32-S3 GPIOs are 3.3V only.

## Quick start

```bash
# One-time: install ESP-IDF v5.5.x and ESP-Matter SDK v1.4.x (~3GB, ~30min)
./scripts/setup.sh

# Each shell: load the toolchain
source ./scripts/activate.sh

# Build, flash, and monitor
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash monitor
```

## Project layout

```
stock-alert/
├── CMakeLists.txt          # Top-level ESP-IDF project
├── partitions.csv          # 8MB flash partition table with Matter NVS
├── sdkconfig.defaults      # Default Kconfig values
├── main/                   # Firmware source
│   ├── CMakeLists.txt
│   ├── idf_component.yml   # esp-matter and other deps
│   ├── app_main.cpp        # Entry point + Matter init
│   ├── stock_sensor.{h,cpp}# VL53L1X driver + hysteresis logic
│   └── matter_handlers.{h,cpp}  # Attribute / identify callbacks
└── scripts/
    ├── setup.sh            # Install toolchain
    └── activate.sh         # Source ESP-IDF + ESP-Matter exports
```

## Commissioning with Apple Home

1. Flash the firmware and watch the serial monitor for the Matter setup QR code.
2. Open the **Maison** app on iPhone → **+** → **Ajouter un accessoire** → scan QR.
3. The device appears as a **Contact Sensor**. Add to your favorite room.
4. Create an automation: *"When sensor opens → send notification / turn on light"*.

## Design notes

- **Semantics:** ContactSensor `open` = low stock (triggers HomeKit alert), `closed` = stock OK.
- **Hysteresis:** distance > `THRESHOLD_LOW_MM` → open; distance < `THRESHOLD_OK_MM` → closed (gap avoids state flapping).
- **Persistence:** thresholds stored in NVS, runtime-tunable via Matter attribute writes (planned).

See `docs/` for deeper architecture notes (TBD).
