// stock-alert firmware entry point.
//
// Boots NVS, brings up Wi-Fi (via SoftAP provisioning on first boot), starts
// the HomeKit accessory exposing a ContactSensor service, and kicks off the
// (currently mocked) stock sampling task. The sensor's state changes flow into
// the HAP data model via homekit::publish_state(), where Apple Home picks them
// up through the local network (no hub required, but a HomePod / Apple TV is
// needed for remote access).

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include <app_wifi.h>
}

#include "include/homekit_accessory.h"
#include "include/stock_alert_config.h"
#include "include/stock_sensor.h"

namespace {

constexpr const char *kTag = "stock_alert";

void on_stock_state_change(stock_alert::sensor::StockState state,
                           void * /*user_data*/) {
    stock_alert::homekit::publish_state(state);
}

}  // namespace

extern "C" void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(kTag, "stock-alert booting (firmware %s)",
             stock_alert::config::kFirmwareVersion);

    // Build the HAP accessory database before bringing Wi-Fi up — hap_start()
    // expects the accessory to already be registered.
    ESP_ERROR_CHECK(stock_alert::homekit::setup_accessory());

    // Bring Wi-Fi up. First boot: starts a SoftAP "STOCKALERT_xxxx" for
    // provisioning via the Espressif "ESP SoftAP Provisioning" app. Subsequent
    // boots: connects directly with stored credentials. Blocks until the
    // station has an IP.
    app_wifi_init();
    ESP_ERROR_CHECK(stock_alert::homekit::start());
    app_wifi_start(portMAX_DELAY);

    // Sensor sampling runs in its own FreeRTOS task; the callback fires every
    // time the debounced state flips and is pushed to HAP from there.
    ESP_ERROR_CHECK(stock_alert::sensor::start(on_stock_state_change, nullptr));

    ESP_LOGI(kTag, "Boot complete. Awaiting Apple Home pairing.");
}
