#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include <lightbulb.h>
// TEST_CASE("PWM", "[Underlying Driver]")
// {
//     //1.Status check
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_R, 4095));
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_rgb_channel(4095, 4095, 0));
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_cctb_or_cw_channel(4095, 4095));
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_rgbcctb_or_rgbcw_channel(4095, 4095, 4095, 4095, 4095));

//     //2. init check
//     driver_pwm_t conf = {
//         .freq_hz = 4000,
//     };
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_init(&conf, NULL));

//     //3. regist Check, step 1
// #if CONFIG_IDF_TARGET_ESP32
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_R, 25));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_G, 26));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_B, 27));
// #else
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_R, 10));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_G, 6));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_B, 7));
// #endif
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_R, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_G, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_B, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgb_channel(4096, 4096, 4096));
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
//     TEST_ESP_ERR(ESP_ERR_INVALID_STATE, pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));

//     //3. regist Check, step 2
// #if CONFIG_IDF_TARGET_ESP32
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 14));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 12));
// #else
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_CCT_COLD, 3));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_regist_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4));
// #endif
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_CCT_COLD, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_channel(PWM_CHANNEL_BRIGHTNESS_WARM, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_cctb_or_cw_channel(4096, 4096));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(4096, 4096, 4096, 4096, 4096));

//     //4. Color check
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_shutdown());
//     vTaskDelay(pdMS_TO_TICKS(100));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(4096, 0, 0, 0, 0));
//     vTaskDelay(pdMS_TO_TICKS(100));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 4096, 0, 0, 0));
//     vTaskDelay(pdMS_TO_TICKS(100));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 4096, 0, 0));
//     vTaskDelay(pdMS_TO_TICKS(100));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 4096, 0));
//     vTaskDelay(pdMS_TO_TICKS(100));
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_rgbcctb_or_rgbcw_channel(0, 0, 0, 0, 4096));

//     //6. deinit
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_set_shutdown());
//     TEST_ASSERT_EQUAL(ESP_OK, pwm_deinit());
// }

esp_err_t led_pwm_rgb_init(void)
{
    lightbulb_config_t config = {
        .type = DRIVER_ESP_PWM,
        .driver_conf.pwm.freq_hz = 5000,
        .capability.enable_fade = true,
        .capability.fade_time_ms = 800,
        .capability.enable_lowpower = false,
        .capability.enable_status_storage = false,
        .capability.led_beads = LED_BEADS_3CH_RGB,
        .capability.storage_cb = NULL,
        .capability.sync_change_brightness_value = true,
        .io_conf.pwm_io.red = BSP_LEDR_GPIO,
        .io_conf.pwm_io.green = BSP_LEDG_GPIO,
        .io_conf.pwm_io.blue = BSP_LEDB_GPIO,
        .external_limit = NULL,
        .gamma_conf = NULL,
        .init_status.mode = WORK_COLOR,
        .init_status.on = true,
        .init_status.hue = 0,
        .init_status.saturation = 100,
        .init_status.value = 100,
    };
    lightbulb_init(&config);
    vTaskDelay(pdMS_TO_TICKS(1000));
    lightbulb_lighting_output_test(LIGHTING_RAINBOW, 1000);
    // TEST_ASSERT_EQUAL(ESP_OK, lightbulb_deinit());
    return ESP_OK;
}
