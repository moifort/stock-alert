#include "include/homekit_accessory.h"

#include <cstdio>
#include <cstring>

#include <esp_log.h>
#include <esp_mac.h>

#include <hap.h>
#include <hap_apple_chars.h>
#include <hap_apple_servs.h>

extern "C" {
#include <app_hap_setup_payload.h>
}

#include "include/stock_alert_config.h"

namespace stock_alert::homekit {

namespace {

constexpr const char *kTag = "homekit_acc";

// HomeKit pairing identifiers. The setup code is what the user types into
// Apple Home; the setup ID is a 4-char tag used by the QR payload and shown
// at the top of the manual pairing card for visual confirmation.
//
// Dev defaults — for production these must be programmed via the
// factory_nvs partition using the `factory_nvs_gen.py` tool from
// esp-homekit-sdk, never hardcoded.
constexpr const char *kSetupCode = "111-22-333";
constexpr const char *kSetupId   = "ES32";

// Cached pointer to the ContactSensorState characteristic — set during
// setup_accessory(), read by publish_state() to push updates.
hap_char_t *s_contact_state_char = nullptr;

int on_identify(hap_acc_t * /*ha*/) {
    // Identify is how Apple Home asks the accessory to "wave" so the user
    // can locate it physically. For now we just log — a follow-up commit
    // will blink the onboard LED.
    ESP_LOGI(kTag, "identify requested");
    return HAP_SUCCESS;
}

// Build a stable serial number from the factory MAC. HomeKit requires a
// unique serial per accessory; deriving it from the MAC avoids collisions
// without needing per-device provisioning.
void format_serial(char *out, size_t out_len) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

}  // namespace

esp_err_t setup_accessory() {
    if (hap_init(HAP_TRANSPORT_WIFI) != HAP_SUCCESS) {
        ESP_LOGE(kTag, "hap_init failed");
        return ESP_FAIL;
    }

    static char serial[13] = {};
    format_serial(serial, sizeof(serial));

    hap_acc_cfg_t cfg = {
        .name             = const_cast<char *>("Stock Alert"),
        .model            = const_cast<char *>("StockSensor-1"),
        .manufacturer     = const_cast<char *>("Stock Alert"),
        .serial_num       = serial,
        .fw_rev           = const_cast<char *>(config::kFirmwareVersion),
        .hw_rev           = const_cast<char *>("1"),
        .pv               = const_cast<char *>("1.1.0"),
        .cid              = HAP_CID_SENSOR,
        .identify_routine = on_identify,
    };
    hap_acc_t *accessory = hap_acc_create(&cfg);
    if (accessory == nullptr) {
        ESP_LOGE(kTag, "hap_acc_create failed");
        return ESP_FAIL;
    }

    // HAP spec R16 requires the Wi-Fi Transport service on Wi-Fi accessories.
    hap_acc_add_wifi_transport_service(accessory, 0);

    // Boot as "stock OK" → DETECTED (0). publish_state() flips to
    // NOT_DETECTED (1) when the sensor reports kLow.
    hap_serv_t *service = hap_serv_contact_sensor_create(0);
    if (service == nullptr) {
        ESP_LOGE(kTag, "hap_serv_contact_sensor_create failed");
        return ESP_FAIL;
    }

    s_contact_state_char =
        hap_serv_get_char_by_uuid(service, HAP_CHAR_UUID_CONTACT_SENSOR_STATE);
    if (s_contact_state_char == nullptr) {
        ESP_LOGE(kTag, "ContactSensorState characteristic missing");
        return ESP_FAIL;
    }

    hap_acc_add_serv(accessory, service);
    hap_add_accessory(accessory);

    // Override the setup code / setup ID stored in factory_nvs with our
    // dev defaults. For production builds, drop these two calls and
    // pre-flash factory_nvs with the salt-verifier pair generated from
    // the real setup code.
    hap_set_setup_code(kSetupCode);
    hap_set_setup_id(kSetupId);
    app_hap_setup_payload(const_cast<char *>(kSetupCode),
                          const_cast<char *>(kSetupId),
                          /* wac_provisioning = */ false,
                          cfg.cid);

    ESP_LOGI(kTag, "ContactSensor accessory ready (serial=%s)", serial);
    return ESP_OK;
}

esp_err_t start() {
    if (hap_start() != HAP_SUCCESS) {
        ESP_LOGE(kTag, "hap_start failed");
        return ESP_FAIL;
    }
    ESP_LOGI(kTag, "HAP started — pair with setup code %s (setup-id %s)",
             kSetupCode, kSetupId);
    return ESP_OK;
}

void publish_state(sensor::StockState state) {
    if (s_contact_state_char == nullptr) {
        ESP_LOGW(kTag, "publish_state called before setup_accessory()");
        return;
    }

    // HAP ContactSensorState: 0 = DETECTED (contact present, stock OK),
    // 1 = NOT_DETECTED (contact lost, stock low).
    const uint8_t value = (state == sensor::StockState::kOk) ? 0 : 1;
    hap_val_t v = {};
    v.u = value;
    hap_char_update_val(s_contact_state_char, &v);
    ESP_LOGI(kTag, "ContactSensorState -> %u (%s)", value,
             value == 0 ? "DETECTED/OK" : "NOT_DETECTED/LOW");
}

}  // namespace stock_alert::homekit
