#include "include/stock_sensor.h"

#include <esp_log.h>

namespace stock_alert::sensor {

namespace {

constexpr const char *kTag = "stock_sensor";

StateChangeCb s_callback   = nullptr;
void         *s_user_data  = nullptr;
StockState    s_state      = StockState::kOk;
bool          s_started    = false;

}  // namespace

esp_err_t start(StateChangeCb callback, void *user_data) {
    s_callback  = callback;
    s_user_data = user_data;
    s_state     = StockState::kOk;
    s_started   = true;
    ESP_LOGI(kTag, "Mock sensor armed — initial state OK, toggle via BOOT button");
    return ESP_OK;
}

void toggle() {
    if (!s_started) {
        ESP_LOGW(kTag, "toggle() called before start()");
        return;
    }
    s_state = (s_state == StockState::kOk) ? StockState::kLow : StockState::kOk;
    ESP_LOGI(kTag, "[MOCK] state -> %s",
             s_state == StockState::kLow ? "LOW (open)" : "OK (closed)");
    if (s_callback != nullptr) {
        s_callback(s_state, s_user_data);
    }
}

}  // namespace stock_alert::sensor
