// stock-alert firmware entry point.
//
// Boots NVS, starts Matter with a single ContactSensor endpoint, and kicks off
// the (currently mocked) stock sampling task. The sensor's state changes flow
// into the Matter data model via matter::publish_state(), where HomeKit picks
// them up through the HomePod mini.

#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>

#include <setup_payload/OnboardingCodesUtil.h>

#include "include/matter_endpoints.h"
#include "include/stock_alert_config.h"
#include "include/stock_sensor.h"

namespace {

constexpr const char *kTag = "stock_alert";

void on_stock_state_change(stock_alert::sensor::StockState state, void * /*user_data*/) {
    stock_alert::matter::publish_state(state);
}

void on_matter_event(const ChipDeviceEvent * /*event*/, intptr_t /*arg*/) {
    // Reserved for commissioning lifecycle hooks (e.g., LED feedback on pair).
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

    ESP_ERROR_CHECK(stock_alert::matter::setup_endpoints());
    ESP_ERROR_CHECK(esp_matter::start(on_matter_event));
    ESP_ERROR_CHECK(stock_alert::sensor::start(on_stock_state_change, nullptr));

    // Surface the QR code and manual pairing code in the serial log so a dev
    // can commission the device without a separate chip-tool invocation.
    // BLE is always available on the ESP32-S3, so we advertise that capability
    // alongside the on-network discovery.
    PrintOnboardingCodes(chip::RendezvousInformationFlags{}
                             .Set(chip::RendezvousInformationFlag::kBLE)
                             .Set(chip::RendezvousInformationFlag::kOnNetwork));

    ESP_LOGI(kTag, "Boot complete. Awaiting Matter commissioning via Apple Home.");
}
