#pragma once

#include <cstdint>
#include <esp_err.h>

// Stock sensor abstraction.
//
// The current implementation is a mock that toggles between "low" and "ok"
// every `kMockTogglePeriodMs`. A real VL53L1X-backed implementation will
// replace stock_sensor.cpp without changing this interface.

namespace stock_alert::sensor {

// Logical stock state. Maps 1:1 to the Matter ContactSensor BooleanState
// attribute: kOk → state_value=true (closed), kLow → state_value=false (open).
enum class StockState : uint8_t {
    kOk  = 0,
    kLow = 1,
};

using StateChangeCb = void (*)(StockState state, void *user_data);

// Starts the sensor sampling task. The callback is invoked from a FreeRTOS
// task context whenever the debounced state flips.
esp_err_t start(StateChangeCb callback, void *user_data);

}  // namespace stock_alert::sensor
