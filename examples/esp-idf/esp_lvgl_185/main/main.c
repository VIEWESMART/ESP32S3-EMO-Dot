/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp/esp-bsp.h"
#include "ui.h"
#include "app_speech_if.h"
#include "app_charging.h"
#include "app_vibration.h"
#include "app_rmaker.h"
#include "app_wifi.h"
#include "app_weather.h"
#include "app_icm.h"
#include "app_tcp_server.h"
#include "app_led.h"

#include "mmap_generate_lottie_assets.h"
#include "thorvg_capi.h"
#include "lv_lottie.h"
#include "esp_lv_decoder.h"
#include "esp_lv_fs.h"

#include "lv_demos.h"

#define LOG_SYSTEM_INFO         (0)
#define LOG_TIME_INTERVAL_MS    (500)

static const char *TAG = "app_main";

mmap_assets_handle_t asset_lottie;
esp_lv_fs_handle_t fs_drive_handle;
esp_lv_decoder_handle_t decoder_handle = NULL;

esp_err_t print_real_time_mem_stats(void);

esp_err_t lv_fs_add(void)
{
    fs_cfg_t fs_cfg;
    fs_cfg.fs_letter = 'A';
    fs_cfg.fs_assets = asset_lottie;
    fs_cfg.fs_nums = MMAP_LOTTIE_ASSETS_FILES;

    esp_err_t ret = esp_lv_fs_desc_init(&fs_cfg, &fs_drive_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize FS");
        return ret;
    }
    return ESP_OK;
}

//***********快速添加画线程序******************* */
static lv_obj_t * canvas;
static lv_point_t last_point;
static bool has_last = false;
static lv_color_t *cbuf = NULL;

// 触摸绘图事件回调
static void draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t point;

    if (indev != NULL) {
        if (code == LV_EVENT_PRESSING) {
            lv_indev_get_point(indev, &point);
            if (has_last) {
                // 绘制一条从 last_point 到当前 point 的线
                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.color = lv_color_black();  // 可以改成其他颜色
                line_dsc.width = 2;

                lv_point_t line_points[] = {last_point, point};
                lv_canvas_draw_line(canvas, line_points, 2, &line_dsc);
            }

            last_point = point;
            has_last = true;
        }
    }

    // 触摸松开后重置
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        has_last = false;
    }
}

void ui_touch_trace_demo(void)
{
    uint16_t hres = lv_disp_get_hor_res(NULL);  // 自动获取屏幕宽度
    uint16_t vres = lv_disp_get_ver_res(NULL);  // 自动获取屏幕高度

    // 创建画布对象
    canvas = lv_canvas_create(lv_scr_act());

    // 分配缓冲区
    cbuf = calloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(hres, vres), sizeof(lv_color_t));
    if (cbuf == NULL) {
        LV_LOG_ERROR("Canvas buffer allocation failed!");
        return;
    }

    // 设置画布缓冲区和大小
    lv_canvas_set_buffer(canvas, cbuf, hres, vres, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);

    // 设置画布背景为白色
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    // 添加交互标志
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

    // 添加触摸绘图事件回调
    lv_obj_add_event_cb(canvas, draw_event_cb, LV_EVENT_ALL, NULL);
}


//****************************** */

void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    bsp_power_init(0);

    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();
    /* Initialize display and LVGL */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    bsp_display_start_with_config(&cfg);

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    const mmap_assets_config_t config_lottie = {
        .partition_label = "animation",
        .max_files = MMAP_LOTTIE_ASSETS_FILES,
        .checksum = MMAP_LOTTIE_ASSETS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config_lottie, &asset_lottie);
    ESP_LOGI(TAG, "[%s]stored_files:%d", config_lottie.partition_label, mmap_assets_get_stored_files(asset_lottie));

    ESP_ERROR_CHECK(lv_fs_add());

    ESP_ERROR_CHECK(esp_lv_decoder_init(&decoder_handle));

    /**
     * @brief Demos provided by LVGL.
     *
     * @note Only enable one demo every time.
     *
     */
    bsp_display_lock(0);
    
    ui_init();
    // lv_demo_widgets();
    // ui_touch_trace_demo();//画线程序

    bsp_display_unlock();

    // vibration_init();

    // power_charging_init();
    // led_pwm_rgb_init();
    // led2812_init();

    // app_rmaker_start();

    // icm42670_init();

    speech_recognition_init();

    static char buffer[2048];
    while (1) {
        // ESP_LOGI(TAG,"BSP_BUTTON: %d", gpio_get_level(BSP_BUTTON));
#if LOG_SYSTEM_INFO
        sprintf(buffer, "\t  Biggest /     Free /    Total\n"
                " SRAM : [%8d / %8d / %8d]\n"
                "PSRAM : [%8d / %8d / %8d]\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        printf("------------ Memory ------------\n");
        printf("%s\n", buffer);

        // ESP_ERROR_CHECK(print_real_time_mem_stats());
        // printf("\n");
#endif

        vTaskDelay(pdMS_TO_TICKS(LOG_TIME_INTERVAL_MS));
    }
}

#if LOG_SYSTEM_INFO
#define ARRAY_SIZE_OFFSET                   8   // Increase this if audio_sys_get_real_time_stats returns ESP_ERR_INVALID_SIZE
#define AUDIO_SYS_TASKS_ELAPSED_TIME_MS  1000   // Period of stats measurement

#define audio_malloc    malloc
#define audio_calloc    calloc
#define audio_free      free
#define AUDIO_MEM_CHECK(tag, x, action) if (x == NULL) { \
        ESP_LOGE(tag, "Memory exhausted (%s:%d)", __FILE__, __LINE__); \
        action; \
    }

const char *task_state[] = {
    "Running",
    "Ready",
    "Blocked",
    "Suspended",
    "Deleted"
};

/** @brief
 * "Extr": Allocated task stack from psram, "Intr": Allocated task stack from internel
 */
const char *task_stack[] = {"Extr", "Intr"};

esp_err_t print_real_time_mem_stats(void)
{
#if (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    uint32_t total_elapsed_time;
    uint32_t task_elapsed_time, percentage_time;
    esp_err_t ret;

    // Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = (TaskStatus_t *)audio_malloc(sizeof(TaskStatus_t) * start_array_size);
    AUDIO_MEM_CHECK(TAG, start_array, {
        ret = ESP_FAIL;
        goto exit;
    });
    // Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ESP_LOGE(TAG, "Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET");
        ret = ESP_FAIL;
        goto exit;
    }

    vTaskDelay(pdMS_TO_TICKS(AUDIO_SYS_TASKS_ELAPSED_TIME_MS));

    // Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = (TaskStatus_t *)audio_malloc(sizeof(TaskStatus_t) * end_array_size);
    AUDIO_MEM_CHECK(TAG, start_array, {
        ret = ESP_FAIL;
        goto exit;
    });

    // Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ESP_LOGE(TAG, "Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET");
        ret = ESP_FAIL;
        goto exit;
    }

    // Calculate total_elapsed_time in units of run time stats clock period.
    total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ESP_LOGE(TAG, "Delay duration too short. Trying increasing AUDIO_SYS_TASKS_ELAPSED_TIME_MS");
        ret = ESP_FAIL;
        goto exit;
    }

    ESP_LOGI(TAG, "| Task              | Run Time    | Per | Prio | HWM       | State   | CoreId   | Stack ");

    // Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {

                task_elapsed_time = end_array[j].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
                percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
                ESP_LOGI(TAG, "| %-17s | %-11d |%2d%%  | %-4u | %-9u | %-7s | %-8x | %s",
                         start_array[i].pcTaskName, (int)task_elapsed_time, (int)percentage_time, start_array[i].uxCurrentPriority,
                         (int)start_array[i].usStackHighWaterMark, task_state[(start_array[i].eCurrentState)],
                         start_array[i].xCoreID, task_stack[esp_ptr_internal(pxTaskGetStackStart(start_array[i].xHandle))]);

                // Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
    }

    // Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            ESP_LOGI(TAG, "| %s | Deleted", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            ESP_LOGI(TAG, "| %s | Created", end_array[i].pcTaskName);
        }
    }
    printf("\n");
    ret = ESP_OK;

exit:    // Common return path
    if (start_array) {
        audio_free(start_array);
        start_array = NULL;
    }
    if (end_array) {
        audio_free(end_array);
        end_array = NULL;
    }
    return ret;
#else
    ESP_LOGW(TAG, "Please enbale `CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID` and `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` in menuconfig");
    return ESP_FAIL;
#endif
}
#endif
