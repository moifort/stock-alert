#include "include/sensor_vl53l1x.h"

#include <driver/i2c_master.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/stock_alert_config.h"

namespace stock_alert::vl53l1x {

namespace {

constexpr const char *kTag = "vl53l1x";

constexpr uint8_t kI2cAddr = 0x29;

// Register addresses we touch directly. The VL53L1X uses 16-bit addresses.
constexpr uint16_t kRegSoftReset                = 0x0000;
constexpr uint16_t kRegFirmwareSystemStatus     = 0x00E5;
constexpr uint16_t kRegIdentificationModelId    = 0x010F;
constexpr uint16_t kRegOscCalibrateVal          = 0x00DE;
constexpr uint16_t kRegVHVConfigTimeoutMacropLoopBound = 0x0008;
constexpr uint16_t kRegPadI2CHvExtsupConfig     = 0x002E;
constexpr uint16_t kRegGpioHvMuxCtrl            = 0x0030;
constexpr uint16_t kRegGpioTioHvStatus          = 0x0031;
constexpr uint16_t kRegSystemInterruptClear     = 0x0086;
constexpr uint16_t kRegSystemModeStart          = 0x0087;
constexpr uint16_t kRegResultRangeStatus        = 0x0089;
constexpr uint16_t kRegResultFinalCrosstalkCorrectedRangeMmSd0 = 0x0096;

constexpr int kI2cXferTimeoutMs   = 100;
constexpr int kBootTimeoutMs      = 1000;
constexpr int kRangingTimeoutMs   = 1000;

i2c_master_bus_handle_t s_bus = nullptr;
i2c_master_dev_handle_t s_dev = nullptr;

// ST "VL53L1_DEFAULT_CONFIGURATION" blob from the ULD (Ultra Lite Driver),
// written starting at register 0x002D. 135 bytes covering SPAD config,
// VHV calibration, default timing, GPIO routing, etc. This is the minimum
// the chip needs to behave correctly — without it ranging returns garbage.
constexpr uint8_t kDefaultConfig[] = {
    0x00, /* 0x2d : set bit 2 and 5 to 1 for fast plus mode (1.8V I/O), else 0 */
    0x00, /* 0x2e : bit 0 if I2C pulled up at 1.8V, 0 if pulled up at AVDD */
    0x00, /* 0x2f : bit 0 if GPIO pulled up at 1.8V, 0 if pulled up at AVDD */
    0x01, /* 0x30 : set bit 4 to 0 for active high interrupt and 1 for active low */
    0x02, /* 0x31 : bit 1 = interrupt depending on the polarity */
    0x00, /* 0x32 : not user-modifiable */
    0x02, /* 0x33 : not user-modifiable */
    0x08, /* 0x34 : not user-modifiable */
    0x00, /* 0x35 : not user-modifiable */
    0x08, /* 0x36 : not user-modifiable */
    0x10, /* 0x37 : not user-modifiable */
    0x01, /* 0x38 : not user-modifiable */
    0x01, /* 0x39 : not user-modifiable */
    0x00, /* 0x3a : not user-modifiable */
    0x00, /* 0x3b : not user-modifiable */
    0x00, /* 0x3c : not user-modifiable */
    0x00, /* 0x3d : not user-modifiable */
    0xff, /* 0x3e : not user-modifiable */
    0x00, /* 0x3f : not user-modifiable */
    0x0F, /* 0x40 : not user-modifiable */
    0x00, /* 0x41 : not user-modifiable */
    0x00, /* 0x42 : not user-modifiable */
    0x00, /* 0x43 : not user-modifiable */
    0x00, /* 0x44 : not user-modifiable */
    0x00, /* 0x45 : not user-modifiable */
    0x20, /* 0x46 : interrupt configuration 0->level low (value=0), 1=level high (value!=0), 2=Out of window, 3=In window, 0x20 = New sample ready */
    0x0b, /* 0x47 : not user-modifiable */
    0x00, /* 0x48 : not user-modifiable */
    0x00, /* 0x49 : not user-modifiable */
    0x02, /* 0x4a : not user-modifiable */
    0x0a, /* 0x4b : not user-modifiable */
    0x21, /* 0x4c : not user-modifiable */
    0x00, /* 0x4d : not user-modifiable */
    0x00, /* 0x4e : not user-modifiable */
    0x05, /* 0x4f : not user-modifiable */
    0x00, /* 0x50 : not user-modifiable */
    0x00, /* 0x51 : not user-modifiable */
    0x00, /* 0x52 : not user-modifiable */
    0x00, /* 0x53 : not user-modifiable */
    0xc8, /* 0x54 : not user-modifiable */
    0x00, /* 0x55 : not user-modifiable */
    0x00, /* 0x56 : not user-modifiable */
    0x38, /* 0x57 : not user-modifiable */
    0xff, /* 0x58 : not user-modifiable */
    0x01, /* 0x59 : not user-modifiable */
    0x00, /* 0x5a : not user-modifiable */
    0x08, /* 0x5b : not user-modifiable */
    0x00, /* 0x5c : not user-modifiable */
    0x00, /* 0x5d : not user-modifiable */
    0x01, /* 0x5e : not user-modifiable */
    0xdb, /* 0x5f : not user-modifiable */
    0x0f, /* 0x60 : not user-modifiable */
    0x01, /* 0x61 : not user-modifiable */
    0xf1, /* 0x62 : not user-modifiable */
    0x0d, /* 0x63 : not user-modifiable */
    0x01, /* 0x64 : Sigma threshold MSB (mm in 14.2 format for MSB+LSB), default 0x1400 = 5mm */
    0x68, /* 0x65 : Sigma threshold LSB */
    0x00, /* 0x66 : Min count Rate MSB (MCPS in 9.7 format for MSB+LSB) */
    0x80, /* 0x67 : Min count Rate LSB */
    0x08, /* 0x68 : not user-modifiable */
    0xb8, /* 0x69 : not user-modifiable */
    0x00, /* 0x6a : not user-modifiable */
    0x00, /* 0x6b : not user-modifiable */
    0x00, /* 0x6c : Intermeasurement period MSB, 32 bits, register changes when using SetInterMeasurementInMs() */
    0x00, /* 0x6d : Intermeasurement period */
    0x0f, /* 0x6e : Intermeasurement period */
    0x89, /* 0x6f : Intermeasurement period LSB */
    0x00, /* 0x70 : not user-modifiable */
    0x00, /* 0x71 : not user-modifiable */
    0x00, /* 0x72 : distance threshold high MSB (mm in 14.2 format) */
    0x00, /* 0x73 : distance threshold high LSB */
    0x00, /* 0x74 : distance threshold low MSB */
    0x00, /* 0x75 : distance threshold low LSB */
    0x00, /* 0x76 : not user-modifiable */
    0x01, /* 0x77 : not user-modifiable */
    0x0f, /* 0x78 : not user-modifiable */
    0x0d, /* 0x79 : not user-modifiable */
    0x0e, /* 0x7a : not user-modifiable */
    0x0e, /* 0x7b : not user-modifiable */
    0x00, /* 0x7c : not user-modifiable */
    0x00, /* 0x7d : not user-modifiable */
    0x02, /* 0x7e : not user-modifiable */
    0xc7, /* 0x7f : ROI center, default 0xC7 = centred */
    0xff, /* 0x80 : XY ROI (X=Width, Y=Height) — default 0xFF = 16x16 */
    0x9B, /* 0x81 : not user-modifiable */
    0x00, /* 0x82 : not user-modifiable */
    0x00, /* 0x83 : not user-modifiable */
    0x00, /* 0x84 : not user-modifiable */
    0x01, /* 0x85 : not user-modifiable */
    0x00, /* 0x86 : clear interrupt, 0x01 = clear */
    0x00  /* 0x87 : start ranging, 0x40 = start, 0x00 = stop */
};

esp_err_t write_byte(uint16_t reg, uint8_t value) {
    const uint8_t buf[3] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
        value,
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), kI2cXferTimeoutMs);
}

esp_err_t write_word(uint16_t reg, uint16_t value) {
    const uint8_t buf[4] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), kI2cXferTimeoutMs);
}

esp_err_t read_byte(uint16_t reg, uint8_t *value) {
    const uint8_t reg_be[2] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
    };
    esp_err_t err = i2c_master_transmit(s_dev, reg_be, sizeof(reg_be), kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    return i2c_master_receive(s_dev, value, 1, kI2cXferTimeoutMs);
}

esp_err_t read_word(uint16_t reg, uint16_t *value) {
    const uint8_t reg_be[2] = {
        static_cast<uint8_t>((reg >> 8) & 0xFF),
        static_cast<uint8_t>(reg & 0xFF),
    };
    uint8_t buf[2] = {};
    esp_err_t err = i2c_master_transmit(s_dev, reg_be, sizeof(reg_be), kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    err = i2c_master_receive(s_dev, buf, sizeof(buf), kI2cXferTimeoutMs);
    if (err != ESP_OK) return err;
    *value = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return ESP_OK;
}

esp_err_t write_block(uint16_t reg, const uint8_t *data, size_t len) {
    // Write the register address (2 bytes BE) followed by the payload in one
    // transaction. The buffer is allocated on the stack — bounded by the size
    // of the default config (~135 bytes).
    uint8_t buf[2 + sizeof(kDefaultConfig)];
    if (len + 2 > sizeof(buf)) return ESP_ERR_INVALID_ARG;
    buf[0] = static_cast<uint8_t>((reg >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(reg & 0xFF);
    for (size_t i = 0; i < len; ++i) buf[2 + i] = data[i];
    return i2c_master_transmit(s_dev, buf, len + 2, kI2cXferTimeoutMs);
}

esp_err_t wait_for_boot() {
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(kBootTimeoutMs);
    uint8_t          status   = 0;
    while (xTaskGetTickCount() < deadline) {
        esp_err_t err = read_byte(kRegFirmwareSystemStatus, &status);
        if (err == ESP_OK && (status & 0x01) != 0) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    ESP_LOGE(kTag, "boot wait timed out (firmware system status never set ready bit)");
    return ESP_ERR_TIMEOUT;
}

}  // namespace

esp_err_t init() {
    // 1. I2C bus + device
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.clk_source                   = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port                     = I2C_NUM_0;
    bus_cfg.scl_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSclGpio);
    bus_cfg.sda_io_num                   = static_cast<gpio_num_t>(stock_alert::config::kI2cSdaGpio);
    bus_cfg.glitch_ignore_cnt            = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), kTag, "bus init");

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = kI2cAddr;
    dev_cfg.scl_speed_hz    = stock_alert::config::kI2cFreqHz;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev),
                        kTag, "add device");

    // 2. Verify model + module + revision (expect EA CC 10)
    uint8_t id[3] = {};
    {
        const uint8_t reg_be[2] = {0x01, 0x0F};
        ESP_RETURN_ON_ERROR(i2c_master_transmit(s_dev, reg_be, 2, kI2cXferTimeoutMs),
                            kTag, "id tx");
        ESP_RETURN_ON_ERROR(i2c_master_receive(s_dev, id, 3, kI2cXferTimeoutMs),
                            kTag, "id rx");
    }
    if (id[0] != 0xEA || id[1] != 0xCC) {
        ESP_LOGE(kTag, "Unexpected ID %02X %02X %02X (expected EA CC <rev>)",
                 id[0], id[1], id[2]);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(kTag, "VL53L1X detected (model=0x%02X module=0x%02X rev=0x%02X)",
             id[0], id[1], id[2]);

    // 3. Wait until the firmware boot flag is set
    ESP_RETURN_ON_ERROR(wait_for_boot(), kTag, "boot wait");

    // 4. Write the default configuration starting at 0x002D
    ESP_RETURN_ON_ERROR(write_block(0x002D, kDefaultConfig, sizeof(kDefaultConfig)),
                        kTag, "default config");

    // 5. Trigger one ranging cycle to lock in the calibration, then stop
    ESP_RETURN_ON_ERROR(write_byte(kRegSystemModeStart, 0x40), kTag, "start ranging (calib)");
    {
        const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(kRangingTimeoutMs);
        uint8_t          gpio     = 0;
        while (xTaskGetTickCount() < deadline) {
            if (read_byte(kRegGpioTioHvStatus, &gpio) == ESP_OK && (gpio & 0x01) == 0) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    ESP_RETURN_ON_ERROR(write_byte(kRegSystemInterruptClear, 0x01), kTag, "clear int (calib)");
    ESP_RETURN_ON_ERROR(write_byte(kRegSystemModeStart, 0x00), kTag, "stop ranging");

    // 6. Finalize VHV calibration retention (per ST ULD VL53L1X_SensorInit)
    ESP_RETURN_ON_ERROR(write_byte(kRegVHVConfigTimeoutMacropLoopBound, 0x09),
                        kTag, "vhv timeout bound");
    ESP_RETURN_ON_ERROR(write_byte(0x000B, 0x00), kTag, "vhv start temp");

    // 7. Keep the default ranging config from the blob (short distance mode,
    //    ~50 ms timing budget, intermeasurement period set in the blob).
    //    Earlier attempts to override into "long mode" produced uniformly
    //    OUT_OF_BOUNDS measurements even with a hand 10 cm away, so we stick
    //    with the well-tested default profile for now.

    // 8. Start ranging continuously — the chip will produce samples per the
    //    default intermeasurement period from the blob. read_distance_mm just
    //    polls data-ready, reads the 16-bit distance, and clears the interrupt.
    ESP_RETURN_ON_ERROR(write_byte(kRegSystemModeStart, 0x40), kTag, "start ranging (live)");

    ESP_LOGI(kTag, "VL53L1X ready (default ranging profile, continuous mode)");
    return ESP_OK;
}

esp_err_t read_distance_with_status(uint16_t *distance_mm, uint8_t *range_status) {
    if (s_dev == nullptr || distance_mm == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(kRangingTimeoutMs);
    uint8_t          gpio     = 0;
    while (true) {
        esp_err_t err = read_byte(kRegGpioTioHvStatus, &gpio);
        if (err != ESP_OK) return err;
        if ((gpio & 0x01) == 1) break;  // data ready (active-high in our config)
        if (xTaskGetTickCount() >= deadline) {
            ESP_LOGW(kTag, "data-ready wait timed out (>%d ms)", kRangingTimeoutMs);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Read range status (0x0089) and distance (0x0096) before clearing.
    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(read_byte(kRegResultRangeStatus, &status), kTag, "read status");
    uint16_t mm = 0;
    ESP_RETURN_ON_ERROR(read_word(kRegResultFinalCrosstalkCorrectedRangeMmSd0, &mm),
                        kTag, "read distance");
    ESP_RETURN_ON_ERROR(write_byte(kRegSystemInterruptClear, 0x01), kTag, "clear int");

    *distance_mm = mm;
    if (range_status != nullptr) *range_status = status;
    return ESP_OK;
}

esp_err_t read_distance_mm(uint16_t *distance_mm) {
    return read_distance_with_status(distance_mm, nullptr);
}

}  // namespace stock_alert::vl53l1x
