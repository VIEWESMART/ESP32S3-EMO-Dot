/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef LED_H
#define LED_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
* @brief Initialize speech recognition
*
* @return
*      - ESP_OK: Create speech recognition task successfully
*/
esp_err_t led_pwm_rgb_init(void);

esp_err_t led2812_set_green(void);

esp_err_t led2812_set_red(void);

esp_err_t led2812_set_blue(void);

#endif
