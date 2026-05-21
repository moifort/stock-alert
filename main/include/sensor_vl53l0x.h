#pragma once

#include <cstdint>
#include <esp_err.h>

// Minimal VL53L0X driver for the stock-alert project.
//
// Scope: enough to verify the chip, trigger a single-shot ranging and read
// the result in millimetres. No continuous mode, no XSHUT switching, no
// inter-measurement period tuning — we sample at low frequency (0.2 Hz) so
// the chip can stay in its default profile.
//
// I2C wiring is fixed by stock_alert_config.h (SDA=GPIO5, SCL=GPIO6,
// 400 kHz, default address 0x29).

namespace stock_alert::vl53l0x {

// Initializes the I2C master bus + the VL53L0X. Verifies the model ID and
// performs the minimal "default mode" warm-up so the next single-shot read
// returns a valid value. Must be called once before read_distance_mm.
esp_err_t init();

// Triggers a single-shot ranging and reads the result in millimetres.
// Returns ESP_OK on success and writes the distance to *distance_mm.
// Returns ESP_ERR_TIMEOUT if the chip does not flag data-ready within a
// fixed window. Reading 8190 mm typically means "out of range" — the chip
// returned an answer but it is unreliable.
esp_err_t read_distance_mm(uint16_t *distance_mm);

}  // namespace stock_alert::vl53l0x
