#pragma once

#include <cstdint>
#include <esp_err.h>

#include "include/stock_sensor.h"

// Matter endpoint setup for the stock-alert device.
//
// Creates a single ContactSensor endpoint (BooleanState cluster) plus the
// implicit Root Node on endpoint 0. The endpoint ID is captured at creation
// time so the sensor task can push state updates into the data model.

namespace stock_alert::matter {

// Creates the ContactSensor endpoint on the given Matter node.
// Returns ESP_OK on success; logs and returns an error otherwise.
esp_err_t setup_endpoints();

// Pushes the latest sensor state into the BooleanState cluster.
// Safe to call from any task once setup_endpoints() has returned ESP_OK.
void publish_state(sensor::StockState state);

}  // namespace stock_alert::matter
