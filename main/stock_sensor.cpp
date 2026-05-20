#include "include/stock_sensor.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/stock_alert_config.h"

namespace stock_alert::sensor {

namespace {

constexpr const char *kTag = "stock_sensor";

struct TaskCtx {
    StateChangeCb callback;
    void *user_data;
};

[[noreturn]] void mock_task(void *arg) {
    auto *ctx = static_cast<TaskCtx *>(arg);
    StockState state = StockState::kOk;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(config::kMockTogglePeriodMs));
        state = (state == StockState::kOk) ? StockState::kLow : StockState::kOk;
        ESP_LOGI(kTag, "[MOCK] flipping state -> %s",
                 state == StockState::kLow ? "LOW (open)" : "OK (closed)");
        if (ctx->callback != nullptr) {
            ctx->callback(state, ctx->user_data);
        }
    }
}

}  // namespace

esp_err_t start(StateChangeCb callback, void *user_data) {
    static TaskCtx ctx{};
    ctx.callback  = callback;
    ctx.user_data = user_data;

    BaseType_t ok = xTaskCreate(mock_task,
                                "stock_sensor",
                                config::kSensorTaskStackBytes,
                                &ctx,
                                config::kSensorTaskPriority,
                                nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "Failed to spawn sensor task");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(kTag, "Mock sensor task started (toggle every %dms)",
             config::kMockTogglePeriodMs);
    return ESP_OK;
}

}  // namespace stock_alert::sensor
