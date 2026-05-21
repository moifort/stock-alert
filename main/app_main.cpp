// stock-alert firmware entry point.
//
// Boots NVS, starts Matter with a single ContactSensor endpoint, arms the
// (currently mocked) sensor, and wires the BOOT button as the Phase 1 mock
// trigger:
//   - short-press → toggle the stock state (DETECTED ↔ NOT_DETECTED)
//   - long-press  → factory reset (wipes commissioning, reboots)

#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/CommissionableDataProvider.h>
#include <setup_payload/OnboardingCodesUtil.h>

#include "include/matter_endpoints.h"
#include "include/mock_button.h"
#include "include/stock_alert_config.h"
#include "include/stock_sensor.h"

namespace {

constexpr const char *kTag = "stock_alert";

void on_stock_state_change(stock_alert::sensor::StockState state, void * /*user_data*/) {
    stock_alert::matter::publish_state(state);
}

// Re-open the commissioning window when the device has no remaining fabric.
// Apple Home closes the window after a successful pair; if the user later
// removes the accessory from the Maison app (without a long-press factory
// reset), we still need to be re-pairable without reflashing.
void open_commissioning_window_if_no_fabric() {
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0) {
        return;
    }
    auto &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    if (mgr.IsCommissioningWindowOpen()) {
        return;
    }
    constexpr auto kWindow = chip::System::Clock::Seconds16(300);
    const CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(
        kWindow, chip::CommissioningWindowAdvertisement::kDnssdOnly);
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(kTag, "OpenBasicCommissioningWindow failed: %" CHIP_ERROR_FORMAT,
                 err.Format());
    } else {
        ESP_LOGI(kTag, "Re-opened commissioning window for 300s after fabric removal");
    }
}

void on_matter_event(const ChipDeviceEvent *event, intptr_t /*arg*/) {
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(kTag, "[commissioning] window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(kTag, "[commissioning] window closed");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(kTag, "[commissioning] PASE session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(kTag, "[commissioning] PASE session stopped");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(kTag, "[commissioning] complete - device on operational fabric");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(kTag, "[commissioning] FAIL - fail-safe timer expired");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(kTag, "[fabric] removed (count=%u)",
                 static_cast<unsigned>(
                     chip::Server::GetInstance().GetFabricTable().FabricCount()));
        open_commissioning_window_if_no_fabric();
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(kTag, "[fabric] will be removed");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(kTag, "[fabric] committed");
        break;
    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(kTag, "[ble] de-initialized, memory reclaimed");
        break;
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(kTag, "[net] interface IP address changed");
        break;
    default:
        break;
    }
}

void on_button_short(void * /*user_data*/) {
    stock_alert::sensor::toggle();
}

void on_button_long(void * /*user_data*/) {
    ESP_LOGW(kTag, "Long-press detected — wiping Matter commissioning and rebooting");
    esp_matter::factory_reset();
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
    ESP_ERROR_CHECK(stock_alert::button::start(on_button_short, on_button_long, nullptr));

    // Surface the QR code and manual pairing code in the serial log so a dev
    // can commission the device without a separate chip-tool invocation.
    // BLE is always available on the ESP32-S3, so we advertise that capability
    // alongside the on-network discovery.
    PrintOnboardingCodes(chip::RendezvousInformationFlags{}
                             .Set(chip::RendezvousInformationFlag::kBLE)
                             .Set(chip::RendezvousInformationFlag::kOnNetwork));

    // Surface the BLE name the device will advertise pre-commissioning, so a
    // dev can match it against what shows up in nRF Connect / LightBlue. The
    // format mirrors what BLEManagerImpl.cpp builds: "<prefix><discriminator>".
    uint16_t discriminator = 0;
    if (chip::DeviceLayer::GetCommissionableDataProvider()->GetSetupDiscriminator(discriminator) ==
        CHIP_NO_ERROR) {
        ESP_LOGI(kTag, "BLE advertising name: %s%04u", CONFIG_BLE_DEVICE_NAME_PREFIX,
                 discriminator);
    }

    ESP_LOGI(kTag, "Boot complete. Awaiting Matter commissioning via Apple Home.");
}
