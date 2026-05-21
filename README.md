# Stock Alert

A small Apple Home accessory that notifies you when something you care about
is running low — coffee beans, dog food, printer paper, screws, spare filters,
soap refills, whatever you don't want to discover empty at the worst moment.
Mount the device above a container, set a threshold, and your iPhone pings
you the way it would for a door being opened.

<!-- TODO photo: device mounted above a container -->
<!-- TODO photo: the Apple Home notification on iPhone -->

> ✅ **Phase 2 (current):** the VL53L1X time-of-flight sensor is wired and
> driving the Apple Home state. Place an object 1-3 cm in front of the lens
> to register it as "present" (Home shows **Closed**); remove it and the
> state flips to "absent" (Home shows **Open**) after ~5 s of polling. The
> BOOT button on the XIAO is still wired and overrides the sensor manually,
> handy for testing automations without moving objects around.

## What it does, in practice

- Shows up in the iOS **Home** app as a **Contact Sensor** (the virtual
  equivalent of a door/window sensor).
- Two states: **`Closed`** = stock OK, **`Open`** = stock low.
- You can build any **automation** Apple Home supports on top of it: push
  notification, scene trigger, LED reminder, voice announcement, message —
  whatever you'd do for a door sensor.
- Talks **Matter over Wi-Fi** — no proprietary hub, no third-party app. Any
  recent HomePod, HomePod mini, or Apple TV 4K acts as the Matter controller.

## Hardware

| Component                                  | Why                                                | ~Price  |
| ------------------------------------------ | -------------------------------------------------- | ------- |
| Seeed Studio **XIAO ESP32-S3**             | Tiny MCU with Wi-Fi + Bluetooth + USB-C            | ~10 €   |
| **U.FL / IPEX 2.4 GHz antenna**            | ⚠️ Practically mandatory (see Troubleshooting)     | ~3 €    |
| **VL53L1X** Time-of-Flight sensor          | Measures distance down to the stock surface (P2)   | ~7 €    |
| 4 female-to-female Dupont wires            | Sensor ↔ board wiring                              | ~1 €    |
| USB-C cable                                | Flashing the firmware                              | likely ✓ |

### Sensor wiring (Phase 2)

| VL53L1X | XIAO ESP32-S3       |
| ------- | ------------------- |
| `VCC`   | `3V3`               |
| `GND`   | `GND`               |
| `SDA`   | `D4` (GPIO 5)       |
| `SCL`   | `D5` (GPIO 6)       |

> ⚠️ Never feed the VL53L1X 5 V — the XIAO GPIOs are 3.3 V only.

<!-- TODO photo: clean wiring of the VL53L1X to the XIAO -->

## Installing from a Mac

### 0. Prerequisites

```bash
# Skip whatever you already have
xcode-select --install
brew install git python@3.13 cmake ninja dfu-util ccache
```

Python 3.10 or newer. No Arduino IDE, no PlatformIO.

### 1. Clone

```bash
git clone https://github.com/moifort/stock-alert.git
cd stock-alert
```

### 2. Install the toolchain (~30 min, ~3 GB)

```bash
./scripts/setup.sh
```

The script downloads and pins ESP-IDF v5.5 + ESP-Matter v1.4 inside
`./toolchain/` (gitignored). One-time cost.

### 3. Load the environment (every fresh shell)

```bash
source ./scripts/activate.sh
```

Expected output:

```
✅ ESP-IDF: ESP-IDF v5.5.3
✅ ESP-Matter at .../toolchain/esp-matter
✅ gn: .../pigweed/gn
```

### 4. Build

```bash
idf.py set-target esp32s3
idf.py build
```

First build takes 5–10 min (Matter is heavy). Incremental rebuilds are
seconds thanks to ninja + ccache.

### 5. Plug the XIAO over USB-C, then flash

```bash
# Port might be /dev/cu.usbmodem2101, /dev/cu.usbmodem1101, etc.
ls /dev/cu.usbmodem*
idf.py -p /dev/cu.usbmodem2101 erase-flash flash monitor
```

The serial monitor prints the boot log and a **Matter QR code**. Keep it
open for the next step.

> To exit the monitor: `Ctrl+]`.

## Pairing with Apple Home

<!-- TODO photo: Matter QR code scanned from the Home app -->

1. Open the **Home** app on your iPhone → **+** → **Add Accessory**.
2. **Scan the QR code** displayed in the serial monitor (or tap "More
   options" and enter the 11-digit code printed just below the QR).
3. Apple Home will say **"Uncertified Matter Accessory"** — that's expected
   and harmless (we use a Matter Test Vendor ID). Tap **Add Anyway**.
4. Apple Home asks which Wi-Fi to use and pushes the credentials to the
   device over Bluetooth.
5. After 1–2 minutes the device shows up as **Stock Sensor** with the
   contact-sensor glyph.

> **Important:** keep the Home app **in the foreground** during the entire
> pairing. Don't lock the iPhone, don't switch to another app. Apple Home
> commissions two Matter fabrics back-to-back (one for your iPhone, one for
> your HomePod), and the second one can time out if attention is lost.

## Day-to-day use

Two ways to drive the state, both reflected in Apple Home within a few
seconds:

### Sensor (primary, automatic)

The VL53L1X polls every **5 seconds**. Distance crosses to/from these
thresholds (defaults, will be NVS-tunable in Phase 3):

| Distance              | Resulting state     | Maison label |
| --------------------- | ------------------- | ------------ |
| < 30 mm               | `OK` (object present)| Closed       |
| 30 – 50 mm            | unchanged (hysteresis band)              |
| > 50 mm               | `LOW` (object absent)| Open         |

The 20 mm hysteresis band avoids flapping around the threshold edge.
End-to-end latency from a stable change to Maison's UI is typically
**1–5 seconds** — same as commercial Matter sensors (Aqara, Eve, …).

### Physical buttons (manual override / debug)

| Gesture                          | Effect                                           |
| -------------------------------- | ------------------------------------------------ |
| Short press (< 3 s) on **B**     | Manually toggle the state, ignoring the sensor   |
| Long press (≥ 3 s) on **B**      | Full reset (wipes the Matter pairing)            |
| Short press on **R**             | Software reboot (keeps the pairing)              |

The buttons are **tiny** SMD components (~1.5 mm × 1.5 mm) to the left of the
Seeed module, near the USB-C connector. Labels `B` and `R` are silkscreened
in white next to them. Press with a fingernail or a retracted pen tip.

## Roadmap

- ✅ **Phase 1** — Matter accessory + physical button driving the state
- ✅ **Phase 2** — VL53L1X wired, periodic distance sampling, automatic
  state changes with hysteresis
- 🔮 **Phase 3** — NVS-backed runtime configuration of thresholds, 3D-printed
  enclosure, magnetic mount, battery + USB-C charging

## Troubleshooting

### Apple Home shows "Accessory Not Responding"

The device is paired but your Apple hub can't reach it on the local network.
Most common cause: **AP isolation** enabled on your Wi-Fi (often the default
on dedicated "IoT" SSIDs). Disable it in your router's admin UI. The device
and the HomePod must be allowed to talk client-to-client on the same SSID,
otherwise Matter cannot work at all.

### Pairing fails, the device shows up in no Wi-Fi scan

Almost always an **unplugged antenna**. The XIAO ESP32-S3 comes with a small
U.FL / IPEX antenna in the box, but the board ships with the RF path routed
to the external connector — without the antenna clipped on, the radio
sensitivity drops by ~30 dB (RSSI around -90 dBm instead of -50).

**Fix:** look inside the original packaging for a tiny flexible antenna with
a small metal connector on a short cable. Clip it onto the gold U.FL socket
on the board (push straight down until you feel it snap). Done. A fresh
Wi-Fi scan after that should show your network well above -65 dBm.

### `Pairing failed` after multiple attempts

1. **Check that a Home hub is active**: Home app → Home Settings → Hubs &
   Bridges → at least one HomePod / Apple TV in **Connected** state. Without
   a hub, Matter accessories cannot be added.
2. **Clear any phantom entries**: if a previous failed attempt left a
   "Matter Accessory" leftover, long-press → Remove Accessory, then retry.
3. **Wipe the device NVS**: `idf.py -p <port> erase-flash flash` for a
   completely fresh state.

### The **B** button doesn't respond

The SMD buttons are very flat. Press squarely in the centre, not on the
edge. The `mock_button` line in the serial monitor confirms detection.

### The sensor reports the same distance no matter what

If `sample mm=` stays constant when you move objects in front of the lens,
check in this order:

1. **Is something physically stuck to the lens?** Solder splatter, flux
   residue or even a stray bit of masking tape will create an internal
   reflection at ~1.5 cm that masks everything beyond.
2. **Are you facing the right side?** The lens is on the *component* side
   of the breakout, not the soldered side. The chip is a small black package
   with a tiny white/grey window.
3. **Is the field of view (~27°) actually pointed at your test object?**
   The chip ignores anything off-axis.

If the I2C scan sees `0x29` but every measurement is `OUT_OF_BOUNDS` or
`HARDWARE_FAIL` even with a clear, well-aimed target, the die is likely
damaged by soldering heat (see the antenna troubleshooting note about the
260 °C / 300-320 °C iron-temperature recommendation).

## Under the hood (for the curious)

- Firmware in **C++17** on **ESP-IDF v5.5** + **ESP-Matter v1.4** (Espressif
  official SDK, code-first — no ZAP file to hand-edit).
- One Matter endpoint: **Contact Sensor** (`BooleanState` cluster `0x0045`).
- Commissioning over BLE + Wi-Fi (Apple Home multi-admin, two fabrics).
- VL53L1X driver based on the ST Ultra Lite Driver default-configuration
  blob, single-bus continuous ranging, hysteresis in software.
- Hardware constants centralised in `main/include/stock_alert_config.h`.
- Project conventions and gotchas documented in `CLAUDE.md`.

## License

TBD.
