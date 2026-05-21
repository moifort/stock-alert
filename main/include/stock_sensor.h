#pragma once

#include <cstdint>
#include <esp_err.h>

// Stock sensor abstraction.
//
// Owns the VL53L1X polling task and applies the compile-time hysteresis from
// stock_alert_config.h to derive the logical stock state. The BOOT button is
// still wired through toggle() as a manual override for dev / debug.

namespace stock_alert::sensor {

// Logical stock state. Maps 1:1 to the Matter ContactSensor BooleanState
// attribute: kOk → state_value=true (closed), kLow → state_value=false (open).
enum class StockState : uint8_t {
    kOk  = 0,
    kLow = 1,
};

using StateChangeCb = void (*)(StockState state, void *user_data);

// Initializes the VL53L1X driver, spawns the polling task and registers the
// state-change callback. The callback is invoked from the polling task when
// the measured distance crosses a hysteresis threshold, and from toggle()
// when the BOOT button override fires.
esp_err_t start(StateChangeCb callback, void *user_data);

// Manually flips the stock state and fires the registered callback, ignoring
// the current sensor reading. Used by the BOOT-button handler.
void toggle();

}  // namespace stock_alert::sensor
