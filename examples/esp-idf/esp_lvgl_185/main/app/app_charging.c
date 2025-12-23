/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/i2c_types.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "ui.h"
#include "app_vibration.h"
#include "app_led.h"
#include "driver/temperature_sensor.h"

const static char *TAG = "app_charging";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
//ADC1 Channels

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_0
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_5

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_11

static int adc_raw[2][10];
static int voltage[2][10];

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_chan0_handle = NULL;
static adc_cali_handle_t adc1_cali_chan1_handle = NULL;

static int st_voltage = 0;
static int st_pre_voltage = 0;
static int st_battery = 0;

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

void chargdecTask(void *arg) //bmi270_config_file
{
    uint8_t reg[3] = {0x08};
    uint8_t data[12];

    while (1) {
        // bq27220
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

void batterydecTask(void *arg)
{
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN1, &config));
    bool do_calibration1_chan1 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN1, EXAMPLE_ADC_ATTEN, &adc1_cali_chan1_handle);
    uint8_t reg[3] = {0x08};
    uint8_t data[12];
    uint16_t voltage=0;
    int16_t current=0;
    int16_t pre_current=0;
    char str_display[10];

    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    float tsens_value;

    while (1) {
        reg[0] = 0x08;
        i2c_master_write_read_device(CONFIG_BSP_I2C_NUM, 0x55, reg, 1,data,2, 1000 / portTICK_PERIOD_MS);  
        // ESP_LOGI(TAG,"Voteage %d",(uint16_t)(data[1]<<8 |data[0])); 
        reg[0] = 0x0c;
        i2c_master_write_read_device(CONFIG_BSP_I2C_NUM, 0x55, reg, 1,&data[2],2, 1000 / portTICK_PERIOD_MS); 
        pre_current = current;
        voltage =  (uint16_t)(data[1]<<8 |data[0]);
        current = (int16_t)(data[3]<<8 |data[2]);
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));

        ESP_LOGI(TAG,"Voteage %d  current %d temperature %f",voltage,current,tsens_value); 

        
        sprintf(str_display, "V= %d mV", voltage);
        lv_label_set_text_fmt(ui_label_voteage, str_display);
        sprintf(str_display, "I= %d mA", current);
        lv_label_set_text_fmt(ui_label_current, str_display);
        sprintf(str_display, "T= %.3f °C", tsens_value);
        lv_label_set_text_fmt(ui_label_temperature, str_display);

        if (current > 5  && pre_current <= -5) {
                ESP_LOGI(TAG, "Wired Charging!");

                vibration_send(2, 800);
                // led2812_set_red();

                // bsp_display_lock(0);
                // _ui_screen_change(&ui_measuing, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_measuing_screen_init);
                // bsp_display_unlock();
        }
        

        // adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN1, &adc_raw[0][1]);
        // ESP_LOGD(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN1, adc_raw[0][1]);
        // if (do_calibration1_chan1) {
        //     adc_cali_raw_to_voltage(adc1_cali_chan1_handle, adc_raw[0][1], &voltage[0][1]);
        //     ESP_LOGD(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN1, voltage[0][1]);
            
        //     st_pre_voltage = st_voltage;
        //     st_voltage = voltage[0][1];
        //     if (st_voltage/100 == 16 && st_pre_voltage/100 != 16) {
        //         ESP_LOGI(TAG, "Wired Charging!");

                
        //         vibration_send(2, 800);
        //         // led2812_set_red();

        //         bsp_display_lock(0);
        //         _ui_screen_change(&ui_measuing, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_measuing_screen_init);
        //         bsp_display_unlock();
        //     }

        //     if(st_pre_voltage == 0 && st_voltage != 0) {
        //         ESP_LOGI(TAG, "Charging Done!");
                
        //         vibration_send(2, 800);
        //         // led2812_set_green();

        //         bsp_display_lock(0);
        //         _ui_screen_change(&ui_watch_digital, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_watch_digital_screen_init);
        //         bsp_display_unlock();
        //     }
        // }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration1_chan1) {
        example_adc_calibration_deinit(adc1_cali_chan1_handle);
    }
}

esp_err_t power_charging_init(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    xTaskCreatePinnedToCore(chargdecTask, "chargdecTask", 3 * 1024, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(batterydecTask, "batterydecTask", 3 * 1024, NULL, 6, NULL, 0);
    return ESP_OK;
}
