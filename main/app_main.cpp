#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "include/stock_alert_config.h"

namespace { constexpr const char *kTag = "i2c_check"; }

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "=== Quick scan to verify hardware still alive ===");
    i2c_master_bus_handle_t bus = nullptr;
    i2c_master_bus_config_t cfg{};
    cfg.clk_source                   = I2C_CLK_SRC_DEFAULT;
    cfg.i2c_port                     = I2C_NUM_0;
    cfg.scl_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSclGpio);
    cfg.sda_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSdaGpio);
    cfg.glitch_ignore_cnt            = 7;
    cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    vTaskDelay(pdMS_TO_TICKS(200));  // give the chip time to come up
    while (true) {
        bool any = false;
        for (uint8_t a = 0x08; a <= 0x77; ++a) {
            if (i2c_master_probe(bus, a, 50) == ESP_OK) {
                ESP_LOGI(kTag, "  found 0x%02X", a);
                any = true;
            }
        }
        if (!any) ESP_LOGW(kTag, "No device responded");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
