#pragma once

#include <cstdint>
#include <esp_err.h>

// Minimal VL53L1X driver for the stock-alert project.
//
// Scope: enough to verify the chip, write the default config blob, set a
// reasonable timing budget, and read single-shot distance measurements in
// millimetres. No continuous mode, no ROI tuning, no XSHUT switching.
//
// I2C wiring is fixed by stock_alert_config.h (SDA=GPIO5, SCL=GPIO6,
// 100 kHz, default address 0x29). The VL53L1X uses 16-bit register
// addresses — registers are sent MSB first.

namespace stock_alert::vl53l1x {

// Initializes the I2C master bus and the VL53L1X chip:
//   - verifies the model+module+revision ID block at 0x010F (EA CC 10)
//   - writes the ST "default configuration" blob to set up SPADs / VHV /
//     ranging defaults
//   - configures a 100 ms timing budget (good accuracy / acceptable rate)
//   - starts ranging in long-distance mode
// Must be called once at boot before read_distance_mm.
esp_err_t init();

// Reads the latest distance measurement in millimetres.
// Returns ESP_OK on success and writes the distance to *distance_mm.
// Returns ESP_ERR_TIMEOUT if no fresh sample is ready within a fixed window.
// A reading near 4000 mm typically means "out of range" (max long distance).
esp_err_t read_distance_mm(uint16_t *distance_mm);

// Same as read_distance_mm but also returns the range status byte from
// register 0x0089. Status == 0 means a valid measurement; any other value
// is the chip's reason code for an invalid range (1=sigma fail, 2=signal
// fail, 4=out of bounds, 7=wraparound, 12=min range fail, etc.).
esp_err_t read_distance_with_status(uint16_t *distance_mm, uint8_t *range_status);

}  // namespace stock_alert::vl53l1x
