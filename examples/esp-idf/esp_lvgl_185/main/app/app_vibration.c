#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_vibration.h"
#include "bsp/bsp_esp_magnescreen.h"
#include "ui.h"

#define TAG "vibration"
static QueueHandle_t vibration_queue = NULL;
void vibration_control(uint32_t target_duty1, uint32_t scale1, uint32_t cycle_num1, uint32_t target_duty2, uint32_t scale2, uint32_t cycle_num2);
static void vibration_task(void *pvParameters)
{
    vibration_message_t receivedMessage;
    int intensity = 0;
    while (1) {
        if (xQueueReceive(vibration_queue, &receivedMessage, portMAX_DELAY) == pdTRUE) {
            intensity = receivedMessage.intensity;
            if (intensity == 0) {
                continue;
            }
#if Z_axis
            switch (receivedMessage.type) {
            case TOUCH:
                vibration_control((int)(intensity * 0.6), (int)(intensity * 0.6), 1, 0, (int)(intensity * 0.2), 2);
                break;
            case PRESS:
                vibration_control((int)(intensity * 0.6), (int)(intensity * 0.6), 1, 0, (int)(intensity * 0.2), 5);
                break;
            case LONG_PRESS:
                vibration_control((int)(intensity * 0.8), (int)(intensity * 0.4), 1, 0, (int)(intensity * 0.15), 5);
                break;
            case SCROLL:
                vibration_control((int)(intensity * 0.6), (int)(intensity * 0.6), 1, 0, (int)(intensity * 0.2), 1);
                break;
            case LONG_VIBRATION:
                vibration_control((int)intensity, (int)intensity, 5, 0, (int)(intensity / 4), 5);
                break;
            }
#else
            switch (receivedMessage.type) {
            case TOUCH:
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, (int)(intensity), 0);
                vTaskDelay(pdMS_TO_TICKS(30));
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0, 0);
                break;
            case PRESS:
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, (int)(intensity), 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0, 0);
                break;
            case LONG_PRESS:
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, (int)(intensity), 0);
                vTaskDelay(pdMS_TO_TICKS(70));
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0, 0);
                break;
            case SCROLL:
                vibration_control((int)(intensity), (int)(intensity), 1, (int)(intensity - 2), 1, 2);
                vibration_control((int)(intensity), 1, 2, 0, (int)(intensity * 0.5), 5);
                break;
            case LONG_VIBRATION:
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, (int)(intensity), 0);
                vTaskDelay(pdMS_TO_TICKS(80));
                vibration_control((int)intensity, (int)intensity, 50, 0, (int)(intensity / 4), 50);
                break;
            }
#endif

        }
    }

    ledc_fade_func_uninstall();
    ledc_stop(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0);
    gpio_reset_pin(BSP_VIBRATION_PIN);
    vQueueDelete(vibration_queue);
    vTaskDelete(NULL);
}

static void up_button_click_cb(void *arg, void *usr_data)
{
    lv_obj_t * cui_daily_mission_arc = lv_obj_get_child(ui_daily_mission_group, 2);
    lv_obj_t * cui_mission_percent = lv_obj_get_child(ui_daily_mission_group, 0);

    int arc_value = lv_arc_get_value(cui_daily_mission_arc) + 10;
    char* mission_percent = (char*)malloc(10);
    sprintf(mission_percent, "%d%%", arc_value);

    bsp_display_lock(0);
    if (arc_value <= 100) {
        lv_arc_set_value(cui_daily_mission_arc, arc_value);
        lv_arc_set_value(ui_volume_arc, arc_value);

        lv_label_set_text(cui_mission_percent, mission_percent);
        lv_label_set_text(ui_volume_percent, mission_percent);
    } else {
        lv_arc_set_value(cui_daily_mission_arc, 0);
        lv_arc_set_value(ui_volume_arc, 0);

        sprintf(mission_percent, "%d%%", 0);
        lv_label_set_text(cui_mission_percent, mission_percent);
        lv_label_set_text(ui_volume_percent, mission_percent);
    }
    bsp_display_unlock();

    vibration_send(2, 800);
    free(mission_percent);
}

static void down_button_click_cb(void *arg, void *usr_data)
{
    lv_obj_t * cui_daily_mission_arc = lv_obj_get_child(ui_daily_mission_group, 2);
    lv_obj_t * cui_mission_percent = lv_obj_get_child(ui_daily_mission_group, 0);

    int arc_value = lv_arc_get_value(cui_daily_mission_arc) - 10;
    char* mission_percent = (char*)malloc(10);
    sprintf(mission_percent, "%d%%", arc_value);

    bsp_display_lock(0);
    if (arc_value >= 0) {
        lv_arc_set_value(cui_daily_mission_arc, arc_value);
        lv_arc_set_value(ui_volume_arc, arc_value);

        lv_label_set_text(cui_mission_percent, mission_percent);
        lv_label_set_text(ui_volume_percent, mission_percent);
    } else {
        lv_arc_set_value(cui_daily_mission_arc, 0);
        lv_arc_set_value(ui_volume_arc, 0);

        sprintf(mission_percent, "%d%%", 0);
        lv_label_set_text(cui_mission_percent, mission_percent);
        lv_label_set_text(ui_volume_percent, mission_percent);
    }
    bsp_display_unlock();

    vibration_send(2, 800);
    free(mission_percent);
}

void ui_event_button_top(lv_event_t * e)
{
    vibration_send(2, 800);
}

void ui_event_button_down(lv_event_t * e)
{
    vibration_send(2, 800);
}

static void vibration_btn_init(void)
{
    // bsp_btn_register_callback(BSP_BUTTON_LEFT, BUTTON_SINGLE_CLICK, up_button_click_cb, NULL);
    // bsp_btn_register_callback(BSP_BUTTON_RIGHT, BUTTON_SINGLE_CLICK, down_button_click_cb, NULL);

    lv_obj_t * cui_daily_mission_arc = lv_obj_get_child(ui_daily_mission_group, 2);
    lv_obj_t * cui_mission_percent = lv_obj_get_child(ui_daily_mission_group, 0);

    int arc_value = lv_arc_get_value(cui_daily_mission_arc);
    char* mission_percent = (char*)malloc(10);
    sprintf(mission_percent, "%d%%", arc_value);

    lv_label_set_text(cui_mission_percent, mission_percent);
    lv_label_set_text(ui_volume_percent, mission_percent);
    lv_obj_add_event_cb(ui_button_top, ui_event_button_top, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_button_down, ui_event_button_down, LV_EVENT_CLICKED, NULL);
}

void vibration_init(void)
{
    bsp_pwm_init();
    vibration_queue = xQueueCreate(5, sizeof(vibration_message_t));
    xTaskCreate(&vibration_task, "vibration_task", 3 * 1024, NULL, 5, NULL);
    // vibration_btn_init();
}

void vibration_deinit(void)
{

}

void vibration_send(vibration_type_t type, vibration_intensity_t intensity)
{
    vibration_message_t receivedMessage;
    receivedMessage.type = type;
    receivedMessage.intensity = intensity;
    xQueueSend(vibration_queue, &receivedMessage, 0);
}

void vibration_control(uint32_t target_duty1, uint32_t scale1, uint32_t cycle_num1, uint32_t target_duty2, uint32_t scale2, uint32_t cycle_num2)
{
    ledc_set_fade_step_and_start(
        LEDC_LOW_SPEED_MODE,
        PWM_CHANNEL,
        target_duty1,     // 设置目标占空比
        scale1,           // 设置步进为负值，即递减
        cycle_num1,       // 定义淡出步进延迟时间
        LEDC_FADE_NO_WAIT // 等待淡出完成
    );
    ledc_set_fade_step_and_start(
        LEDC_LOW_SPEED_MODE,
        PWM_CHANNEL,
        target_duty2,  // 设置目标占空比
        scale2,  // 设置步进为负值，即递减
        cycle_num2, // 定义淡出步进延迟时间
        LEDC_FADE_NO_WAIT  // 等待淡出完成
    );
}