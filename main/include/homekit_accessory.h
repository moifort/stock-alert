#pragma once

#include <esp_err.h>

#include "stock_sensor.h"

// HomeKit (HAP) accessory façade.
//
// Owns the lifecycle of a single HAP accessory exposing a ContactSensor
// service. The stock sensor's logical state is mapped to the
// ContactSensorState characteristic (HAP 0x6A): kOk → DETECTED,
// kLow → NOT_DETECTED.

namespace stock_alert::homekit {

// Builds the HAP accessory + ContactSensor service and registers them with
// the HomeKit database. Must be called once before start().
esp_err_t setup_accessory();

// Starts the HAP core (mDNS announce, pair-setup loop, etc.). Call after
// Wi-Fi is up and setup_accessory() has succeeded.
esp_err_t start();

// Pushes the current stock state to the ContactSensor characteristic.
// Safe to call from any task; HAP serializes updates internally.
void publish_state(sensor::StockState state);

}  // namespace stock_alert::homekit
