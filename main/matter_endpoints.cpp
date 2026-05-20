#include "include/matter_endpoints.h"

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_attribute_utils.h>

#include "include/stock_alert_config.h"

namespace stock_alert::matter {

namespace {

constexpr const char *kTag = "matter_ep";

uint16_t s_contact_endpoint_id = 0;

esp_err_t on_attribute_update(esp_matter::attribute::callback_type_t type,
                              uint16_t endpoint_id,
                              uint32_t cluster_id,
                              uint32_t attribute_id,
                              esp_matter_attr_val_t *val,
                              void *priv_data) {
    if (type == esp_matter::attribute::PRE_UPDATE) {
        ESP_LOGD(kTag, "pre-update ep=%u cluster=0x%08lx attr=0x%08lx",
                 endpoint_id,
                 static_cast<unsigned long>(cluster_id),
                 static_cast<unsigned long>(attribute_id));
    }
    return ESP_OK;
}

esp_err_t on_identification(esp_matter::identification::callback_type_t type,
                            uint16_t endpoint_id,
                            uint8_t effect_id,
                            uint8_t effect_variant,
                            void *priv_data) {
    // Identify is how Apple Home asks the device to "wave" so the user can
    // locate it physically. For now we just log — a follow-up commit will
    // blink the onboard LED.
    ESP_LOGI(kTag, "identify ep=%u type=%d effect=%u variant=%u",
             endpoint_id, type, effect_id, effect_variant);
    return ESP_OK;
}

}  // namespace

esp_err_t setup_endpoints() {
    using namespace esp_matter;
    using namespace esp_matter::endpoint;

    node::config_t node_config;
    node_t *node = node::create(&node_config, on_attribute_update, on_identification);
    if (node == nullptr) {
        ESP_LOGE(kTag, "node::create failed");
        return ESP_FAIL;
    }

    contact_sensor::config_t contact_config;
    // BooleanState.StateValue: true = closed, false = open.
    // We boot as "stock OK" → closed.
    contact_config.boolean_state.state_value = true;

    endpoint_t *ep = contact_sensor::create(node, &contact_config,
                                            ENDPOINT_FLAG_NONE, nullptr);
    if (ep == nullptr) {
        ESP_LOGE(kTag, "contact_sensor::create failed");
        return ESP_FAIL;
    }

    s_contact_endpoint_id = endpoint::get_id(ep);
    ESP_LOGI(kTag, "ContactSensor endpoint id=%u", s_contact_endpoint_id);
    return ESP_OK;
}

void publish_state(sensor::StockState state) {
    using namespace chip::app::Clusters;

    if (s_contact_endpoint_id == 0) {
        ESP_LOGW(kTag, "publish_state called before endpoint setup");
        return;
    }

    // ContactSensor sem: closed (true) when stock is OK, open (false) when low.
    const bool closed = (state == sensor::StockState::kOk);
    esp_matter_attr_val_t val = esp_matter_bool(closed);
    esp_matter::attribute::update(s_contact_endpoint_id,
                                  BooleanState::Id,
                                  BooleanState::Attributes::StateValue::Id,
                                  &val);
    ESP_LOGI(kTag, "BooleanState -> %s", closed ? "closed (OK)" : "open (LOW)");
}

}  // namespace stock_alert::matter
