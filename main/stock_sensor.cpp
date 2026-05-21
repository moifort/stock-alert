#include "include/stock_sensor.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/sensor_vl53l1x.h"
#include "include/stock_alert_config.h"

namespace stock_alert::sensor {

namespace {

constexpr const char *kTag = "stock_sensor";

StateChangeCb s_callback   = nullptr;
void         *s_user_data  = nullptr;
StockState    s_state      = StockState::kOk;
bool          s_started    = false;
bool          s_have_sensor = false;

void apply_state(StockState new_state, const char *reason) {
    if (new_state == s_state) {
        return;
    }
    s_state = new_state;
    ESP_LOGI(kTag, "state -> %s (%s)",
             s_state == StockState::kLow ? "LOW (open)" : "OK (closed)",
             reason);
    if (s_callback != nullptr) {
        s_callback(s_state, s_user_data);
    }
}

// Continuously polls the VL53L1X every kSensorPollPeriodMs and applies the
// hysteresis defined in stock_alert_config.h. Invalid samples are ignored so
// transient out-of-range readings do not flip the state.
[[noreturn]] void poll_task(void * /*arg*/) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(config::kSensorPollPeriodMs));

        uint16_t mm = 0;
        uint8_t  raw_status = 0xFF;
        esp_err_t err = vl53l1x::read_distance_with_status(&mm, &raw_status);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "vl53l1x read failed: %s", esp_err_to_name(err));
            continue;
        }

        // Drop anything the chip flagged as suspect — only raw=9 (mapped to
        // VALID) and raw=12 (range valid + min check) are trustworthy.
        const uint8_t raw = raw_status & 0x1F;
        if (raw != 9 && raw != 12) {
            ESP_LOGD(kTag, "ignoring sample mm=%u raw_status=%u", mm, raw);
            continue;
        }

        if (mm > config::kThresholdLowMmDefault) {
            apply_state(StockState::kLow, "distance above LOW threshold");
        } else if (mm < config::kThresholdOkMmDefault) {
            apply_state(StockState::kOk, "distance below OK threshold");
        }
        // Else: inside the hysteresis band (OK..LOW), keep the previous state.

        ESP_LOGI(kTag, "sample mm=%u state=%s",
                 mm, s_state == StockState::kLow ? "LOW" : "OK");
    }
}

}  // namespace

esp_err_t start(StateChangeCb callback, void *user_data) {
    s_callback  = callback;
    s_user_data = user_data;
    s_state     = StockState::kOk;
    s_started   = true;

    esp_err_t err = vl53l1x::init();
    if (err != ESP_OK) {
        ESP_LOGE(kTag,
                 "vl53l1x::init failed: %s — falling back to BOOT-button mock",
                 esp_err_to_name(err));
        s_have_sensor = false;
        return ESP_OK;  // Still let the rest of the firmware boot.
    }
    s_have_sensor = true;
    ESP_LOGI(kTag,
             "VL53L1X armed (poll every %d ms, hysteresis %d/%d mm)",
             config::kSensorPollPeriodMs, config::kThresholdOkMmDefault,
             config::kThresholdLowMmDefault);

    BaseType_t ok = xTaskCreate(poll_task, "stock_poll",
                                config::kSensorTaskStackBytes, nullptr,
                                config::kSensorTaskPriority, nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "xTaskCreate failed for poll_task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void toggle() {
    if (!s_started) {
        ESP_LOGW(kTag, "toggle() called before start()");
        return;
    }
    // The BOOT button remains a manual override for dev / smoke-testing the
    // Matter pipeline even when the sensor is active.
    const StockState next = (s_state == StockState::kOk) ? StockState::kLow
                                                          : StockState::kOk;
    apply_state(next, "BOOT button");
}

}  // namespace stock_alert::sensor
