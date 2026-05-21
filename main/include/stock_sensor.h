#pragma once

#include <cstdint>
#include <esp_err.h>

// Stock sensor abstraction.
//
// The current implementation is a mock backed by the BOOT button: each short
// press flips the state between kOk and kLow. A real VL53L1X-backed
// implementation will replace stock_sensor.cpp without changing this interface.

namespace stock_alert::sensor {

// Logical stock state. Maps 1:1 to the Matter ContactSensor BooleanState
// attribute: kOk → state_value=true (closed), kLow → state_value=false (open).
enum class StockState : uint8_t {
    kOk  = 0,
    kLow = 1,
};

using StateChangeCb = void (*)(StockState state, void *user_data);

// Registers the state-change callback and initializes the mock state to kOk.
// The callback is invoked synchronously from toggle().
esp_err_t start(StateChangeCb callback, void *user_data);

// Flips the mocked stock state and fires the registered callback.
// No-op if start() has not been called.
void toggle();

}  // namespace stock_alert::sensor
