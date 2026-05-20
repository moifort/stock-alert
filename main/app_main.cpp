// stock-alert firmware entry point.
//
// This file is the entry point for the firmware. It boots the chip, configures
// the LED, initializes Matter, exposes a single ContactSensor endpoint, and
// starts the (currently mocked) stock sampling task.
//
// The Matter endpoint is the heart of the UX: state=open means low stock,
// state=closed means stock OK. Apple Home picks this up via the HomePod mini.

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

static const char *TAG = "stock_alert";

extern "C" void app_main(void)
{
    // Initialize NVS — required by Wi-Fi, Matter, and our own threshold storage.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "stock-alert booted (firmware v%s)", "0.1.0-skeleton");
    ESP_LOGI(TAG, "Matter and sensor init pending — see follow-up commits.");
}
