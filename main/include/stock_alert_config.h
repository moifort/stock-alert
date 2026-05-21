#pragma once

// Centralized compile-time configuration for the stock-alert firmware.
// Runtime-tunable values live in NVS — see stock_sensor::load_thresholds().

namespace stock_alert::config {

// Firmware version exposed in logs and Matter BasicInformation cluster.
constexpr const char *kFirmwareVersion = "0.1.0-mock";

// VL53L1X I2C wiring on the XIAO ESP32-S3.
// D4 = GPIO5 (SDA), D5 = GPIO6 (SCL) per Seeed pinout.
constexpr int kI2cSdaGpio = 5;
constexpr int kI2cSclGpio = 6;
// 100 kHz is the safest for the VL53L0X breakout — observed 400 kHz timeouts
// on the CJMCU-style boards we use, likely due to weak external pull-ups.
constexpr int kI2cFreqHz  = 100'000;

// Distance thresholds (mm) for ContactSensor state transitions.
// Use case: the sensor sits close to the object it tracks (e.g. lid above a
// jar, or detecting presence on a shelf), so "present" means the object is
// 1-3 cm away and "absent" means there is nothing nearby for several cm.
// Hysteresis: state opens (low stock / object absent) when distance crosses
// LOW; it closes again only when distance drops below OK.
// Defaults — overridden by NVS at runtime once persistence lands.
constexpr int kThresholdLowMmDefault = 50;   // > 5 cm → absent / stock low
constexpr int kThresholdOkMmDefault  = 30;   // < 3 cm → present / stock OK

// BOOT button on the XIAO ESP32-S3 Sense (GPIO 0, pulled HIGH; pressed = LOW).
// Used as the Phase 1 mock trigger: short-press toggles the stock state,
// long-press wipes Matter commissioning so the device can be re-paired.
constexpr int kButtonGpio         = 0;
constexpr int kButtonPollPeriodMs = 20;     // debouncing poll cadence
constexpr int kButtonDebounceMs   = 50;     // ignore presses shorter than this
constexpr int kButtonLongPressMs  = 3000;   // factory-reset threshold

// VL53L1X polling cadence: one ranging every 5 s (0.2 Hz). Cheap, plenty
// fast for stock monitoring, and leaves the chip idle most of the time —
// important when we move to battery power.
constexpr int kSensorPollPeriodMs = 5000;

// FreeRTOS task tuning. Stack must accommodate the I2C reads, hysteresis
// logic, and the state-change callback that schedules a Matter attribute
// update via ScheduleLambda — kept at 6 KB to match the button task.
constexpr int kSensorTaskStackBytes = 6144;
constexpr int kSensorTaskPriority   = 5;
// Stack must accommodate the on_button_short callback which schedules a Matter
// attribute update through the SDK — observed >4 KB consumption in practice.
constexpr int kButtonTaskStackBytes = 6144;
constexpr int kButtonTaskPriority   = 5;

}  // namespace stock_alert::config
