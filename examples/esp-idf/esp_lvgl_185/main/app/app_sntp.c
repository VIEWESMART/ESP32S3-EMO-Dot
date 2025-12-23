/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "app_wifi.h"
#include "ui.h"

static const char *TAG = "sntp";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
RTC_DATA_ATTR static int boot_count = 0;

static void obtain_time(void);
static void initialize_sntp(void);

#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
    settimeofday(tv, NULL);
    ESP_LOGI(TAG, "Time is synchronized from custom code");
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

void timeupdateTask(void *arg)
{
    time_t now;
    struct tm timeinfo;

    char str_hour1[5];
    char str_hour2[5];
    char str_min[15];
    char month_buffer[10];
    char date_buffer[10];
    char year_buffer[20];

    while(1) {
        if(app_wifi_is_connected()) {
            time(&now);
            localtime_r(&now, &timeinfo);
            ESP_LOGD(TAG, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

            sprintf(str_hour1, "%d", timeinfo.tm_hour / 10);
            sprintf(str_hour2, "%d", timeinfo.tm_hour % 10);
            if(timeinfo.tm_min < 10) {
                sprintf(str_min, "0%d", timeinfo.tm_min);
            } else {
                sprintf(str_min, "%d", timeinfo.tm_min);
            }
            
            lv_label_set_text_fmt(ui_label_hour_1, str_hour1);
            lv_label_set_text_fmt(ui_label_hour_2, str_hour2);
            lv_label_set_text_fmt(ui_label_min, str_min);
            if(timeinfo.tm_hour / 10 == 1) {
                lv_obj_set_x(ui_label_hour_1, 28);
            } else {
                lv_obj_set_x(ui_label_hour_1, -23);
            }

            strftime(date_buffer,  sizeof(date_buffer), "%d %a", &timeinfo);
            strftime(month_buffer, sizeof(month_buffer), "%b", &timeinfo);
            strftime(year_buffer, sizeof(year_buffer), "%Y", &timeinfo);

            lv_label_set_text_fmt(ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_MONTH), month_buffer);
            lv_label_set_text_fmt(ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_YEAR), year_buffer);
            lv_label_set_text_fmt(ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_DAY), date_buffer);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event, sec=%lu", tv->tv_sec);
    settimeofday(tv, NULL);
}

void app_sntp_init(void)
{
//     ++boot_count;
//     ESP_LOGI(TAG, "Boot count: %d", boot_count);

//     time_t now;
//     struct tm timeinfo;
//     time(&now);
//     localtime_r(&now, &timeinfo);
//     // Set timezone to China Standard Time
//     setenv("TZ", "CST-8", 1);
//     tzset();
//     // Is time set? If not, tm_year will be (1970 - 1900).
//     if (timeinfo.tm_year < (2016 - 1900)) {
//         ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
//         obtain_time();
//         // update 'now' variable with current time
//         time(&now);
//     }
// #ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
//     else {
//         // add 500 ms error to the current system time.
//         // Only to demonstrate a work of adjusting method!
//         {
//             ESP_LOGI(TAG, "Add a error for test adjtime");
//             struct timeval tv_now;
//             gettimeofday(&tv_now, NULL);
//             int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
//             int64_t error_time = cpu_time + 500 * 1000L;
//             struct timeval tv_error = {.tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L};
//             settimeofday(&tv_error, NULL);
//         }

//         ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
//         obtain_time();
//         // update 'now' variable with current time
//         time(&now);
//     }
// #endif

//     char strftime_buf[64];
//     localtime_r(&now, &timeinfo);
//     strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
//     ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    xTaskCreatePinnedToCore(timeupdateTask, "timeupdateTask", 4 * 1024, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, "app_sntp_init finished");

    // if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
    //     struct timeval outdelta;
    //     while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
    //         adjtime(NULL, &outdelta);
    //         ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
    //                  (long)outdelta.tv_sec,
    //                  outdelta.tv_usec / 1000,
    //                  outdelta.tv_usec % 1000);
    //         vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     }
    // }
}

static void obtain_time(void)
{

    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "time.asia.apple.com");
    esp_sntp_setservername(2, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    esp_sntp_init();
}