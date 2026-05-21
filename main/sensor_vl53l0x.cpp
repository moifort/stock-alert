#include "include/sensor_vl53l0x.h"

#include <driver/i2c_master.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/stock_alert_config.h"

namespace stock_alert::vl53l0x {

namespace {

constexpr const char *kTag = "vl53l0x";

// I2C address of the VL53L0X (factory default).
constexpr uint8_t kI2cAddr = 0x29;

// Subset of registers we actually touch. Names follow STMicro's documentation.
constexpr uint8_t kRegSysrangeStart            = 0x00;
constexpr uint8_t kRegResultInterruptStatus    = 0x13;
constexpr uint8_t kRegResultRangeStatus        = 0x14;
constexpr uint8_t kRegResultRangeMilliMeterHi  = 0x1E;  // 16-bit big-endian
constexpr uint8_t kRegSysInterruptClear        = 0x0B;
constexpr uint8_t kRegSysInterruptConfigGpio   = 0x0A;
constexpr uint8_t kRegIdentificationModelId    = 0xC0;
constexpr uint8_t kModelIdExpected             = 0xEE;

constexpr int kRangingTimeoutMs = 200;
constexpr int kI2cXferTimeoutMs = 100;

i2c_master_bus_handle_t s_bus = nullptr;
i2c_master_dev_handle_t s_dev = nullptr;

esp_err_t write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), kI2cXferTimeoutMs);
}

esp_err_t read_reg(uint8_t reg, uint8_t *value) {
    // Split write + read instead of transmit_receive: some chips (incl. the
    // VL53L0X) misbehave with the restart condition and the new ESP-IDF v5.x
    // master API returns ESP_ERR_INVALID_STATE in that case.
    esp_err_t err = i2c_master_transmit(s_dev, &reg, 1, kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    return i2c_master_receive(s_dev, value, 1, kI2cXferTimeoutMs);
}

esp_err_t read_reg16(uint8_t reg, uint16_t *value) {
    uint8_t buf[2] = {};
    esp_err_t err = i2c_master_transmit(s_dev, &reg, 1, kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    err = i2c_master_receive(s_dev, buf, sizeof(buf), kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    *value = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return ESP_OK;
}

}  // namespace

esp_err_t init() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.clk_source                   = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port                     = I2C_NUM_0;
    bus_cfg.scl_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSclGpio);
    bus_cfg.sda_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSdaGpio);
    bus_cfg.glitch_ignore_cnt            = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), kTag, "i2c bus init");
    ESP_LOGI(kTag, "bus created (SDA=GPIO%d, SCL=GPIO%d)",
             stock_alert::config::kI2cSdaGpio, stock_alert::config::kI2cSclGpio);

    // Probe before adding a device handle (matches the scan firmware which
    // worked reliably).
    esp_err_t probe_err = i2c_master_probe(s_bus, kI2cAddr, 100);
    ESP_LOGI(kTag, "probe(0x%02X) -> %s", kI2cAddr, esp_err_to_name(probe_err));
    if (probe_err != ESP_OK) {
        return probe_err;
    }

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = kI2cAddr;
    dev_cfg.scl_speed_hz    = stock_alert::config::kI2cFreqHz;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), kTag, "i2c add device");
    ESP_LOGI(kTag, "device handle ready (addr=0x%02X @ %d Hz)",
             kI2cAddr, stock_alert::config::kI2cFreqHz);

    // Confirm we are talking to a real VL53L0X.
    uint8_t model_id = 0;
    esp_err_t mid_err = read_reg(kRegIdentificationModelId, &model_id);
    ESP_LOGI(kTag, "read_reg(0xC0) -> %s, value=0x%02X",
             esp_err_to_name(mid_err), model_id);
    ESP_RETURN_ON_ERROR(mid_err, kTag, "read model id");
    if (model_id != kModelIdExpected) {
        ESP_LOGE(kTag, "Unexpected model ID 0x%02X (expected 0x%02X) — wrong chip or wiring",
                 model_id, kModelIdExpected);
        return ESP_ERR_INVALID_STATE;
    }

    // Configure GPIO1 interrupt to fire when a new sample is ready. We do not
    // wire the physical INT line, but the interrupt status register is still
    // the canonical "data ready" flag.
    ESP_RETURN_ON_ERROR(write_reg(kRegSysInterruptConfigGpio, 0x04), kTag, "interrupt cfg");
    ESP_RETURN_ON_ERROR(write_reg(kRegSysInterruptClear, 0x01), kTag, "interrupt clear");

    ESP_LOGI(kTag, "VL53L0X ready (I2C 0x%02X, model 0x%02X)", kI2cAddr, model_id);
    return ESP_OK;
}

esp_err_t read_distance_mm(uint16_t *distance_mm) {
    if (s_dev == nullptr || distance_mm == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // Kick off a single ranging measurement.
    ESP_RETURN_ON_ERROR(write_reg(kRegSysrangeStart, 0x01), kTag, "start ranging");

    // Poll the interrupt status register until bit 0 is set (data ready).
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(kRangingTimeoutMs);
    uint8_t          int_status = 0;
    while (true) {
        ESP_RETURN_ON_ERROR(read_reg(kRegResultInterruptStatus, &int_status),
                            kTag, "poll interrupt");
        if ((int_status & 0x07) != 0) {
            break;  // any of the three "ready" flavours
        }
        if (xTaskGetTickCount() >= deadline) {
            ESP_LOGW(kTag, "ranging timed out (>%d ms)", kRangingTimeoutMs);
            // Best-effort: clear the interrupt so the next call is not skewed.
            (void)write_reg(kRegSysInterruptClear, 0x01);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // The 16-bit distance lives two bytes after the status register.
    uint16_t mm = 0;
    ESP_RETURN_ON_ERROR(read_reg16(kRegResultRangeMilliMeterHi, &mm), kTag, "read distance");
    ESP_RETURN_ON_ERROR(write_reg(kRegSysInterruptClear, 0x01), kTag, "clear interrupt");

    *distance_mm = mm;
    return ESP_OK;
}

}  // namespace stock_alert::vl53l0x
