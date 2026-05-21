#include "include/mock_button.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/stock_alert_config.h"

namespace stock_alert::button {

namespace {

constexpr const char *kTag = "mock_button";

struct TaskCtx {
    ShortPressCb on_short;
    LongPressCb  on_long;
    void        *user_data;
};

[[noreturn]] void poll_task(void *arg) {
    auto *ctx = static_cast<TaskCtx *>(arg);

    bool    pressed        = false;
    int64_t press_start_us = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(config::kButtonPollPeriodMs));

        const bool now_pressed =
            gpio_get_level(static_cast<gpio_num_t>(config::kButtonGpio)) == 0;

        if (now_pressed && !pressed) {
            press_start_us = esp_timer_get_time();
            pressed        = true;
            continue;
        }

        if (!now_pressed && pressed) {
            const int64_t duration_ms =
                (esp_timer_get_time() - press_start_us) / 1000;
            pressed = false;

            if (duration_ms < config::kButtonDebounceMs) {
                continue;  // contact bounce, ignore
            }

            if (duration_ms >= config::kButtonLongPressMs) {
                ESP_LOGI(kTag, "Long-press (%lldms) -> long callback", duration_ms);
                if (ctx->on_long != nullptr) {
                    ctx->on_long(ctx->user_data);
                }
            } else {
                ESP_LOGI(kTag, "Short-press (%lldms) -> short callback", duration_ms);
                if (ctx->on_short != nullptr) {
                    ctx->on_short(ctx->user_data);
                }
            }
        }
    }
}

}  // namespace

esp_err_t start(ShortPressCb on_short, LongPressCb on_long, void *user_data) {
    gpio_config_t io = {};
    io.pin_bit_mask  = 1ULL << config::kButtonGpio;
    io.mode          = GPIO_MODE_INPUT;
    io.pull_up_en    = GPIO_PULLUP_ENABLE;
    io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io.intr_type     = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    static TaskCtx ctx{};
    ctx.on_short  = on_short;
    ctx.on_long   = on_long;
    ctx.user_data = user_data;

    BaseType_t ok = xTaskCreate(poll_task,
                                "mock_button",
                                config::kButtonTaskStackBytes,
                                &ctx,
                                config::kButtonTaskPriority,
                                nullptr);
    if (ok != pdPASS) {
        ESP_LOGE(kTag, "xTaskCreate failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag,
             "BOOT button on GPIO %d (short < %dms, long >= %dms)",
             config::kButtonGpio,
             config::kButtonLongPressMs,
             config::kButtonLongPressMs);
    return ESP_OK;
}

}  // namespace stock_alert::button
