

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_io_expander_new_i2c_tca9554(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, esp_io_expander_handle_t *handle_ret);

#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000    (0x20)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_001    (0x21)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_010    (0x22)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_011    (0x23)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_100    (0x24)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_101    (0x25)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_110    (0x26)
#define ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_111    (0x27)

#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000    (0x38)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_001    (0x39)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_010    (0x3A)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_011    (0x3B)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_100    (0x3C)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_101    (0x3D)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_110    (0x3E)
#define ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_111    (0x3F)

#ifdef __cplusplus
}
#endif