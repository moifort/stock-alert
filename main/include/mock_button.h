#pragma once

#include <esp_err.h>

// BOOT button driver (GPIO 0 on the XIAO ESP32-S3).
//
// Two distinct gestures:
//   - Short-press (>= debounce, < long-press threshold): on_short callback.
//   - Long-press  (>= long-press threshold):              on_long callback.
//
// Phase 1 wiring:
//   - on_short → toggle the mocked stock state
//   - on_long  → esp_matter::factory_reset() (wipes commissioning, reboots)

namespace stock_alert::button {

using ShortPressCb = void (*)(void *user_data);
using LongPressCb  = void (*)(void *user_data);

// Configures the GPIO, spawns a debouncing poll task, and dispatches gestures
// to the given callbacks. Either callback may be nullptr.
esp_err_t start(ShortPressCb on_short, LongPressCb on_long, void *user_data);

}  // namespace stock_alert::button
