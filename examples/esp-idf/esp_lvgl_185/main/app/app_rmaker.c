/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Unlicense OR CC0-1.0
*/

#include <string.h>
#include <inttypes.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_common_events.h>

#include "bsp/esp-bsp.h"
#include "ui.h"

#include <app_wifi.h>

static const char *TAG = "app_rmaker";

static esp_err_t magic_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                   const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{

    return ESP_OK;
}

static void rainmaker_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
        case RMAKER_EVENT_REBOOT:
            ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
            break;
        case RMAKER_EVENT_WIFI_RESET:
            ESP_LOGI(TAG, "Wi-Fi credentials reset.");
            break;
        case RMAKER_EVENT_FACTORY_RESET:
            ESP_LOGI(TAG, "Node reset to factory defaults.");
            break;
        case RMAKER_MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected.");
            /* The UI can perform some actions */
            break;
        case RMAKER_MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected.");
            /* The UI can perform some actions */
            break;
        case RMAKER_MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
            break;
        default:
            ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32"", event_id);
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

static void print_node_config(void)
{
    extern char *esp_rmaker_get_node_config(void);
    char *cfg = esp_rmaker_get_node_config();
    if (cfg) {
        ESP_LOGI(TAG, "%s", cfg);
        free(cfg);
    } else {
        ESP_LOGE(TAG, "get node config failed");
    }
}

static void rmaker_task(void *args)
{
    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();

    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &rainmaker_event_handler, NULL));

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "esp.device.magic");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    esp_rmaker_device_t *magic_device = esp_rmaker_device_create("ESP RainMaker Device", "esp.device.magic", NULL);
    esp_rmaker_device_add_param(magic_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "ESP RainMaker Device"));
    esp_rmaker_device_add_cb(magic_device, magic_write_cb, NULL);
    esp_rmaker_node_add_device(node, magic_device);

    /* Enable timezone service. */
    esp_rmaker_timezone_service_enable();

    /* Enable schduel service. */
    esp_rmaker_schedule_enable();

    esp_rmaker_system_serv_config_t serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reset_reboot_seconds = 2,
    };
    esp_rmaker_system_service_enable(&serv_config);

    /* Start rmaker core. */
    esp_rmaker_start();

    print_node_config();

    esp_err_t err = app_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi");
    }

    vTaskDelete(NULL);
}

void wifi_credential_reset()
{
    ESP_LOGW(TAG, "WiFi credential reset");
    esp_rmaker_wifi_reset(0, 2);
    esp_rmaker_factory_reset(0, 2);
    esp_restart();
}

esp_err_t app_rmaker_start(void)
{
    // bsp_btn_register_callback(BSP_BUTTON_POWER, BUTTON_LONG_PRESS_START, wifi_credential_reset, NULL);

    BaseType_t ret_val = xTaskCreatePinnedToCore(rmaker_task, "RMaker Task", 6 * 1024, NULL, 1, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret_val) ? ESP_OK : ESP_FAIL);
    return ESP_OK;
}
