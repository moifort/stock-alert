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
constexpr int kI2cFreqHz  = 400'000;

// Distance thresholds (mm) for ContactSensor state transitions.
// Hysteresis: state opens (low stock) at LOW; closes again only when below OK.
// Defaults — overridden by NVS at runtime once persistence lands.
constexpr int kThresholdLowMmDefault = 220;
constexpr int kThresholdOkMmDefault  = 180;

// BOOT button on the XIAO ESP32-S3 Sense (GPIO 0, pulled HIGH; pressed = LOW).
// Used as the Phase 1 mock trigger: short-press toggles the stock state,
// long-press wipes Matter commissioning so the device can be re-paired.
constexpr int kButtonGpio         = 0;
constexpr int kButtonPollPeriodMs = 20;     // debouncing poll cadence
constexpr int kButtonDebounceMs   = 50;     // ignore presses shorter than this
constexpr int kButtonLongPressMs  = 3000;   // factory-reset threshold

// FreeRTOS task tuning.
constexpr int kSensorTaskStackBytes = 4096;
constexpr int kSensorTaskPriority   = 5;
// Stack must accommodate the on_button_short callback which schedules a Matter
// attribute update through the SDK — observed >4 KB consumption in practice.
constexpr int kButtonTaskStackBytes = 6144;
constexpr int kButtonTaskPriority   = 5;

}  // namespace stock_alert::config
