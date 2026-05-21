# stock-alert

HomeKit-over-Wi-Fi stock level sensor for Apple Home. A XIAO ESP32-S3 reads a VL53L1X
time-of-flight sensor mounted above a container and exposes a HomeKit ContactSensor that
goes to "not detected" when the stock falls below a configurable threshold.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-S3 Sense (ESP32-S3, 8MB flash, 8MB PSRAM, native USB)
- **Sensor:** VL53L1X ToF (I2C, range 4cm–400cm)
- **HomeKit hub (optional):** HomePod mini, Apple TV, or HomePod 2nd gen — required only for
  remote access and automations; local pairing works without a hub.

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
# One-time: install ESP-IDF v5.5.x and esp-homekit-sdk (~1GB, ~10min)
./scripts/setup.sh

# Each shell: load the toolchain
source ./scripts/activate.sh

# Build, flash, and monitor
idf.py set-target esp32s3
idf.py erase-flash    # only needed when migrating from a previous Matter build
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash monitor
```

## Project layout

```
stock-alert/
├── CMakeLists.txt           # Top-level ESP-IDF project (pulls in HAP via HOMEKIT_PATH)
├── partitions.csv           # 8MB flash partition table (single app + factory_nvs)
├── sdkconfig.defaults       # Default Kconfig values
├── main/                    # Firmware source
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   ├── app_main.cpp                 # Entry point + Wi-Fi + HAP init
│   ├── stock_sensor.{h,cpp}         # VL53L1X driver + hysteresis logic
│   └── homekit_accessory.{h,cpp}    # HAP accessory + ContactSensor service
└── scripts/
    ├── setup.sh             # Install toolchain
    └── activate.sh          # Source ESP-IDF + export HOMEKIT_PATH
```

## Pairing with Apple Home

1. Flash the firmware. The device starts a Wi-Fi access point named **`STOCKALERT_xxxx`**.
2. On iPhone, install the **ESP SoftAP Provisioning** app (Espressif). Open it, scan for the
   device, and pass it your home Wi-Fi SSID + password.
3. Once the device is on your network, open **Home → +** → **Add Accessory** → **More options**.
   `Stock Alert` should appear in the "Nearby Accessories" list.
4. Enter the HomeKit setup code **`111-22-333`** (printed in the serial log on boot).
5. The device appears as a **Contact Sensor**. Add to your favorite room.

## Design notes

- **Semantics:** ContactSensorState `DETECTED` (0) = stock OK, `NOT_DETECTED` (1) = stock low.
- **Hysteresis:** distance > `THRESHOLD_LOW_MM` → low; distance < `THRESHOLD_OK_MM` → ok
  (gap avoids state flapping).
- **Persistence:** thresholds stored in NVS, runtime-tunable via local HTTP endpoint (planned).
- **No BLE:** HomeKit pairing is Wi-Fi only, and provisioning uses SoftAP — so BT/NimBLE is
  disabled entirely, freeing flash and RAM.
