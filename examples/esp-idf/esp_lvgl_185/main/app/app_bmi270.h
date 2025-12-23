// /*
//  * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
//  *
//  * SPDX-License-Identifier: Apache-2.0
//  */

// #pragma once

// #ifdef __cplusplus
// extern "C" {
// #endif

// #include "driver/i2c.h"

// #define I2C_MASTER_SCL_IO           5      /*!< GPIO number used for I2C master clock */
// #define I2C_MASTER_SDA_IO           4      /*!< GPIO number used for I2C master data  */
// #define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
// #define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
// #define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
// #define I2C_MASTER_TIMEOUT_MS       1000

// #define MPU9250_SENSOR_ADDR                 0x68        /*!< Slave address of the MPU9250 sensor */


// #define BMI270_I2C_ADDRESS         0x68 /*!< I2C address with AD0 pin low */

// typedef struct {
//     float x;
//     float y;
//     float z;
// } bmi270_value_t;

// esp_err_t bmi270_init(void);
// extern const uint8_t bmi270_config_file[];



// #ifdef __cplusplus
// }
// #endif
