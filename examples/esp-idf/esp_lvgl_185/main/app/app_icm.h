// /*
//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */

// #ifndef ICM_H
// #define ICM_H

// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"

// typedef struct {
//     uint8_t status;
//     int16_t x;
//     int16_t y;
//     int16_t width;
//     int16_t size_w;
//     int16_t size_h;
// } icm_button_t;

// typedef struct {
//     uint8_t status;
//     int16_t line_x;
//     int16_t line_y;
//     int16_t ball_x;
//     int16_t ball_y;
//     uint8_t turn;
//     uint8_t ball_pos;
// } icm_espnow_quenedata_t;

// typedef struct {
//     float x;
//     float y;
//     float z;
//     float velocity_x;
//     float velocity_y;
//     float velocity_z;
//     float size;
//     float transparency;
// } ball_t;
// /**
// * @brief Initialize speech recognition
// *
// * @return
// *      - ESP_OK: Create speech recognition task successfully
// */
// esp_err_t icm42670_init(void);

// #endif
