# stock-alert

Matter-over-Wi-Fi stock level sensor for Apple HomeKit. A XIAO ESP32-S3 reads a VL53L1X
time-of-flight sensor mounted above a container and exposes a Matter ContactSensor that
"opens" when the stock falls below a configurable threshold.

> **Phase 1 (current):** the sensor is mocked — the BOOT button on the XIAO board toggles
> the stock state on each short press. The real VL53L1X driver lands in Phase 2.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-S3 Sense (ESP32-S3, 8MB flash, 8MB PSRAM, native USB)
- **Sensor:** VL53L1X ToF (I2C, range 4cm–400cm) — wired but not yet read in Phase 1
- **Matter controller:** Apple HomePod mini (or Apple TV / HomePod 2nd gen) on the same Wi-Fi

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

## Pairing with Apple Home (Matter)

1. Flash the firmware and watch the serial monitor. The boot log prints the Matter
   setup QR code (ASCII) **and** an 11-digit manual pairing code.
2. On iPhone : open the **Maison** app → **+** → **Ajouter un accessoire** → scan
   the QR code directly from the terminal screen (or paste the manual code via
   "Plus d'options").
3. Apple Home will warn "Accessoire non certifié" — this is expected in dev (the
   device uses Matter test VID/PID). Tap **Ajouter quand même**.
4. Apple Home asks for the Wi-Fi credentials and pushes them to the device over
   BLE. Commissioning typically completes in 30–60 s.
5. The device appears as a **Contact Sensor**. Add it to a room and create
   automations as you like.

## BOOT button (Phase 1 mock)

The BOOT button on the XIAO board doubles as the mock trigger:

| Gesture                | Action                                                |
| ---------------------- | ----------------------------------------------------- |
| Short press (< 3 s)    | Toggle the stock state (DETECTED ↔ NOT_DETECTED)     |
| Long press  (≥ 3 s)    | Factory reset — wipes Matter commissioning + reboots |

After a factory reset, remove the accessory from the Maison app, then rescan the
QR code from the serial log to re-pair.

## Project layout

```
stock-alert/
├── CMakeLists.txt          # Top-level ESP-IDF project
├── partitions.csv          # 8MB flash partition table with Matter NVS
├── sdkconfig.defaults      # Default Kconfig values
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml       # esp-matter and other deps
│   ├── app_main.cpp            # Entry point + Matter init + button wiring
│   ├── matter_endpoints.{h,cpp}# ContactSensor endpoint + attribute updates
│   ├── mock_button.{h,cpp}     # BOOT button driver (short + long-press)
│   └── stock_sensor.{h,cpp}    # State holder (real VL53L1X comes in Phase 2)
└── scripts/
    ├── setup.sh            # Install toolchain
    └── activate.sh         # Source ESP-IDF + ESP-Matter exports
```

## Design notes

- **Semantics:** ContactSensor `closed` (BooleanState=true) = stock OK,
  `open` (BooleanState=false) = stock low. Apple Home shows "Détecté" / "Non détecté".
- **Commissioning:** BLE for the initial pairing handshake, Wi-Fi afterwards.
  An Apple Home hub (HomePod mini, HomePod 2, Apple TV 4K) is required to add the
  accessory permanently and enable remote access.
- **Hysteresis (Phase 2):** distance > `THRESHOLD_LOW_MM` → open; distance <
  `THRESHOLD_OK_MM` → closed. The gap avoids state flapping.
- **Persistence:** thresholds will be stored in NVS and runtime-tunable.
