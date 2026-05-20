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

// Mock toggle period — used until the real VL53L1X driver is wired in.
constexpr int kMockTogglePeriodMs = 30'000;

// FreeRTOS task tuning.
constexpr int kSensorTaskStackBytes = 4096;
constexpr int kSensorTaskPriority   = 5;

}  // namespace stock_alert::config
