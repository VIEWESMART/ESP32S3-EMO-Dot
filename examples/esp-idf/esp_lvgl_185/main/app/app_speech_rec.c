/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "spi_flash_mmap.h"
#include "driver/i2s_std.h"
#include "xtensa/core-macros.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "dl_lib_coefgetter_if.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "app_speech_if.h"
#include "bsp/bsp_esp_magnescreen.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"
#include "esp_mn_speech_commands.h"
#include "audio_player.h"
#include "esp_random.h"
#include "file_manager.h"
#include "esp_alc.h"
#include "ui.h"

#include "mmap_generate_lottie_assets.h"
#include "thorvg_capi.h"
#include "lv_lottie.h"
#include "esp_lv_decoder.h"
#include "esp_lv_fs.h"

static const char *TAG = "speeech recognition";

typedef struct {
    sr_cb_t fn;   /*!< function */
    void *args;   /*!< function args */
} func_t;

static func_t g_sr_callback_func[SR_CB_TYPE_MAX] = {0};

static i2s_chan_handle_t rx_chan;        // I2S rx channel handler
static i2s_chan_handle_t tx_chan;        // I2S tx device handler

extern mmap_assets_handle_t asset_lottie;

#define COMMANDS_NUM 9
const sr_cmd_t g_default_cmd_info[] = {
    // English
    {"Switch On the Light", "da kai dian deng"},
    {"Switch Off the Light", "guan bi dian deng"},
    {"Switch On the Fan", "da kai feng shan"},
    {"Switch Off the Fan", "guan bi feng shan"},
    {"Switch On the Air", "da kai kong tiao"},
    {"Switch Off the Air", "guan bi kong tiao"},
    {"Network setup", "pei wang"},
    {"IR learning", "hong wai xue xi"},
    {"Home return", "fan hui zhu ye"},
};

#define SPIFFS_BASE       "/spiffs"

typedef enum {
    AUDIO_WAKE,
    AUDIO_OK,
    AUDIO_END,
    AUDIO_MAX,
} audio_segment_t;

typedef struct {
    uint8_t *audio_buffer;
    size_t len;
} audio_data_t;

typedef struct {
    // The "RIFF" chunk descriptor
    uint8_t ChunkID[4];
    int32_t ChunkSize;
    uint8_t Format[4];
    // The "fmt" sub-chunk
    uint8_t Subchunk1ID[4];
    int32_t Subchunk1Size;
    int16_t AudioFormat;
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;
    int16_t BlockAlign;
    int16_t BitsPerSample;
    // The "data" sub-chunk
    uint8_t Subchunk2ID[4];
    int32_t Subchunk2Size;
} wav_header_t;

// static audio_data_t g_audio_data[AUDIO_MAX];

static void *volume_handle;

static int get_volume_level(void)
{
    lv_obj_t * cui_daily_mission_arc = lv_obj_get_child(ui_daily_mission_group, 2);

    int arc_value = lv_arc_get_value(cui_daily_mission_arc);
    ESP_LOGD(TAG, "arc_value: %d", arc_value);
    
    if(arc_value >= 100) {
        return 10;
    } else if(arc_value >= 90) {
        return 5;
    } else if(arc_value >= 80) {
        return 0;
    } else if(arc_value >= 70) {
        return -5;
    } else if(arc_value >= 60) {
        return -10;
    } else if(arc_value >= 50) {
        return -15;
    } else if(arc_value >= 40) {
        return -20;
    } else if(arc_value >= 30) {
        return -25;
    } else if(arc_value >= 20) {
        return -27;
    } else if(arc_value >= 10) {
        return -30;
    } else {
        return -40;
    }
}

esp_err_t app_audio_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;

    alc_volume_setup_process(audio_buffer, len, 1, volume_handle, get_volume_level());
    ESP_LOGD(TAG, "level: %d", get_volume_level());
    if (i2s_channel_write(tx_chan, audio_buffer, len, bytes_written, timeout_ms) != ESP_OK) {
    // if (i2s_channel_write_expand(tx_chan, audio_buffer, len, I2S_DATA_BIT_WIDTH_16BIT, I2S_DATA_BIT_WIDTH_32BIT, bytes_written, timeout_ms) != ESP_OK) {
        ESP_LOGE(TAG, "Write Task: i2s write failed");
        ret = ESP_FAIL;
    }

    return ret;
}
 
static void sr_wake(void *arg)
{
    // lv_label_set_text(ui_avatar_label, "I'm listening...");
    bsp_display_lock(0);
    _ui_screen_change(&ui_call, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, &ui_call_screen_init);
    ui_send_sys_event(ui_call, LV_EVENT_FACE_LOOK, NULL);
    bsp_display_unlock();

    FILE *fp = NULL;
    char *audio_file = (char*)heap_caps_calloc(1, 48, MALLOC_CAP_SPIRAM);
    strncpy(audio_file, "/spiffs/echo_cn_wake.wav", 48);
    fp = fopen(audio_file, "rb");
    if(fp == NULL) {
        ESP_LOGE(TAG, "Open file %s failed", audio_file);
    }
    size_t file_size = fm_get_file_size(audio_file);

    audio_data_t g_audio_data;
    g_audio_data.len = file_size;
    g_audio_data.audio_buffer = (uint8_t*)heap_caps_calloc(1, file_size, MALLOC_CAP_SPIRAM);
    fread(g_audio_data.audio_buffer, 1, file_size, fp);

    uint8_t *p = g_audio_data.audio_buffer;
    wav_header_t *wav_head = (wav_header_t *)p;
    if (NULL == strstr((char *)wav_head->Subchunk1ID, "fmt") &&
            NULL == strstr((char *)wav_head->Subchunk2ID, "data")) {
        ESP_LOGE(TAG, "Header of wav format error");
    }
    p += sizeof(wav_header_t);
    size_t len = g_audio_data.len - sizeof(wav_header_t);
    len = len & 0xfffffffc;
    ESP_LOGD(TAG, "frame_rate=%d, ch=%d, width=%d", wav_head->SampleRate, wav_head->NumChannels, wav_head->BitsPerSample);
    size_t bytes_written = 0;

    app_audio_write((void*)p, len, &bytes_written, portMAX_DELAY);
    // vTaskDelay(pdMS_TO_TICKS(20));
    heap_caps_free(audio_file);
    heap_caps_free(g_audio_data.audio_buffer);
    fclose(fp);
}

static void sr_cmd(void *arg)
{
    int command_id = (int)arg;
    ESP_LOGI(TAG, "command_id: %d", command_id);

    lv_label_set_text(ui_avatar_label, g_default_cmd_info[command_id].str);

    if (strcmp(g_default_cmd_info[command_id].phoneme, "pei wang") == 0) {

        _ui_screen_change(&ui_ScreenNet, LV_SCR_LOAD_ANIM_FADE_ON, 100, 500, &ui_ScreenNet_screen_init);

    } else if (strcmp(g_default_cmd_info[command_id].phoneme, "hong wai xue xi") == 0) {

        _ui_screen_change(&ui_ScreenIR, LV_SCR_LOAD_ANIM_FADE_ON, 100, 500, &ui_ScreenIR_screen_init);

    } else if (strcmp(g_default_cmd_info[command_id].phoneme, "fan hui zhu ye") == 0) {

        _ui_screen_change(&ui_watch_digital, LV_SCR_LOAD_ANIM_FADE_ON, 100, 500, &ui_watch_digital_screen_init);

    }

    FILE *fp = NULL;
    char *audio_file = (char*)heap_caps_calloc(1, 48, MALLOC_CAP_SPIRAM);
    strncpy(audio_file, "/spiffs/echo_cn_ok.wav", 48);
    fp = fopen(audio_file, "rb");
    if(fp == NULL) {
        ESP_LOGE(TAG, "Open file %s failed", audio_file);
    }
    size_t file_size = fm_get_file_size(audio_file);

    audio_data_t g_audio_data;
    g_audio_data.len = file_size;
    g_audio_data.audio_buffer = (uint8_t*)heap_caps_calloc(1, file_size, MALLOC_CAP_SPIRAM);
    fread(g_audio_data.audio_buffer, 1, file_size, fp);

    uint8_t *p = g_audio_data.audio_buffer;
    wav_header_t *wav_head = (wav_header_t *)p;

    if (NULL == strstr((char *)wav_head->Subchunk1ID, "fmt") &&
            NULL == strstr((char *)wav_head->Subchunk2ID, "data")) {
        ESP_LOGE(TAG, "Header of wav format error");
    }
    p += sizeof(wav_header_t);
    size_t len = g_audio_data.len - sizeof(wav_header_t);
    len = len & 0xfffffffc;
    ESP_LOGD(TAG, "frame_rate=%d, ch=%d, width=%d", wav_head->SampleRate, wav_head->NumChannels, wav_head->BitsPerSample);
    size_t bytes_written = 0;

    app_audio_write((void*)p, len, &bytes_written, portMAX_DELAY);
    // vTaskDelay(pdMS_TO_TICKS(20));
    heap_caps_free(g_audio_data.audio_buffer);
    heap_caps_free(audio_file);
    fclose(fp);
}

static void sr_cmd_exit(void *arg)
{
    // lv_label_set_text(ui_avatar_label, "Timeout");
    _ui_screen_change(&ui_watch_digital, LV_SCR_LOAD_ANIM_FADE_ON, 100, 1000, &ui_watch_digital_screen_init);

    FILE *fp = NULL;
    char *audio_file = (char*)heap_caps_calloc(1, 48, MALLOC_CAP_SPIRAM);
    strncpy(audio_file, "/spiffs/echo_cn_end.wav", 48);
    fp = fopen(audio_file, "rb");
    if(fp == NULL) {
        ESP_LOGE(TAG, "Open file %s failed", audio_file);
    }
    size_t file_size = fm_get_file_size(audio_file);

    audio_data_t g_audio_data;
    g_audio_data.len = file_size;
    g_audio_data.audio_buffer = (uint8_t*)heap_caps_calloc(1, file_size, MALLOC_CAP_SPIRAM);
    fread(g_audio_data.audio_buffer, 1, file_size, fp);

    uint8_t *p = g_audio_data.audio_buffer;
    wav_header_t *wav_head = (wav_header_t *)p;

    if (NULL == strstr((char *)wav_head->Subchunk1ID, "fmt") &&
            NULL == strstr((char *)wav_head->Subchunk2ID, "data")) {
        ESP_LOGE(TAG, "Header of wav format error");
    }
    p += sizeof(wav_header_t);
    size_t len = g_audio_data.len - sizeof(wav_header_t);
    len = len & 0xfffffffc;
    ESP_LOGD(TAG, "frame_rate=%d, ch=%d, width=%d", wav_head->SampleRate, wav_head->NumChannels, wav_head->BitsPerSample);
    size_t bytes_written = 0;

    app_audio_write((void*)p, len, &bytes_written, portMAX_DELAY);
    // vTaskDelay(pdMS_TO_TICKS(20));
    heap_caps_free(g_audio_data.audio_buffer);
    heap_caps_free(audio_file);
    fclose(fp);
}

static esp_err_t audio_player_mute(AUDIO_PLAYER_MUTE_SETTING setting)
{
    return ESP_OK;
}

static esp_err_t audio_player_reconfig_std(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    return ESP_OK;
}

static esp_err_t audio_player_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{

    return i2s_channel_write(tx_chan, audio_buffer, len, bytes_written, timeout_ms);
    // return i2s_channel_write_expand(tx_chan, audio_buffer, len, I2S_DATA_BIT_WIDTH_16BIT, I2S_DATA_BIT_WIDTH_32BIT, bytes_written, timeout_ms);
}

void recsrcTask(void *arg)
{
    ESP_ERROR_CHECK(i2s_init());
    ESP_ERROR_CHECK(get_i2s_rx_chan(&rx_chan));
    ESP_ERROR_CHECK(get_i2s_tx_chan(&tx_chan));

    volume_handle = alc_volume_setup_open();
    if (volume_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create ALC handle. (line %d)", __LINE__);
    }

    srmodel_list_t *models = esp_srmodel_init("model");
    const char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL); // select WakeNet model
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE); // select MultiNet model
    const esp_wn_iface_t *wakenet = esp_wn_handle_from_name(wn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_wn_data = wakenet->create(wn_name, DET_MODE_90);
    ESP_LOGI(TAG, "model_wn_data = %p", model_wn_data);
    int wn_num = wakenet->get_word_num(model_wn_data);
    float wn_threshold = 0;
    int wn_sample_rate = wakenet->get_samp_rate(model_wn_data);
    int audio_wn_chunksize = wakenet->get_samp_chunksize(model_wn_data);
    ESP_LOGI(TAG, "keywords_num = %d, threshold = %f, sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", wn_num, wn_threshold, wn_sample_rate, audio_wn_chunksize, sizeof(int16_t));

    model_iface_data_t *model_mn_data = multinet->create(mn_name, 6000);
    esp_mn_commands_clear();
    for (int i = 0; i < COMMANDS_NUM; i++) {
        esp_mn_commands_add(i, g_default_cmd_info[i].phoneme);
    }
    esp_mn_commands_update();

    int audio_mn_chunksize = multinet->get_samp_chunksize(model_mn_data);
    int mn_num = multinet->get_samp_chunknum(model_mn_data);
    int mn_sample_rate = multinet->get_samp_rate(model_mn_data);
    ESP_LOGI(TAG, "keywords_num = %d , sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", mn_num,  mn_sample_rate, audio_mn_chunksize, sizeof(int16_t));

    int size = audio_wn_chunksize;

    if (audio_mn_chunksize > audio_wn_chunksize) {
        size = audio_mn_chunksize;
    }

    int * buffer = (int *)heap_caps_malloc(size * 2 * sizeof(int), MALLOC_CAP_SPIRAM);
    bool enable_wn = true;

    size_t read_len = 0;
    while (1) {
        i2s_channel_read(rx_chan, buffer, size * 2 * sizeof(int), &read_len, portMAX_DELAY);
        for (int x = 0; x < size * 2 / 4; x++) {
            int s1 = ((buffer[x * 4] + buffer[x * 4 + 1]) >> 13) & 0x0000FFFF;
            int s2 = ((buffer[x * 4 + 2] + buffer[x * 4 + 3]) << 3) & 0xFFFF0000;
            buffer[x] = s1 | s2;
        }

        if (enable_wn) {

            wakenet_state_t r = wakenet->detect(model_wn_data, (int16_t *)buffer);

            if (r == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "%s DETECTED", wakenet->get_word_name(model_wn_data, r));

                if (NULL != g_sr_callback_func[SR_CB_TYPE_WAKE].fn) {
                    g_sr_callback_func[SR_CB_TYPE_WAKE].fn(g_sr_callback_func[SR_CB_TYPE_WAKE].args);
                }

                enable_wn = false;
            }
        } else {
            esp_mn_state_t mn_state = multinet->detect(model_mn_data, (int16_t *)buffer);

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_mn_data);
                ESP_LOGI(TAG, "MN test successfully, Commands ID: %d", mn_result->phrase_id[0]);
                int command_id = mn_result->phrase_id[0];

                if (NULL != g_sr_callback_func[SR_CB_TYPE_CMD].fn) {
                    if (NULL != g_sr_callback_func[SR_CB_TYPE_CMD].args) {
                        g_sr_callback_func[SR_CB_TYPE_CMD].fn(g_sr_callback_func[SR_CB_TYPE_CMD].args);
                    } else {
                        g_sr_callback_func[SR_CB_TYPE_CMD].fn((void *)command_id);
                    }
                }

            } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_mn_data);
                ESP_LOGI(TAG, "timeout, string:%s\n", mn_result->string);

                if (NULL != g_sr_callback_func[SR_CB_TYPE_CMD_EXIT].fn) {
                    g_sr_callback_func[SR_CB_TYPE_CMD_EXIT].fn(g_sr_callback_func[SR_CB_TYPE_CMD_EXIT].args);
                }

                enable_wn = true;
            } else {
                continue;
            }
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t sr_handler_install(sr_cb_type_t type, sr_cb_t handler, void *args)
{
    switch (type) {
    case SR_CB_TYPE_WAKE:
        g_sr_callback_func[SR_CB_TYPE_WAKE].fn = handler;
        g_sr_callback_func[SR_CB_TYPE_WAKE].args = args;
        break;

    case SR_CB_TYPE_CMD:
        g_sr_callback_func[SR_CB_TYPE_CMD].fn = handler;
        g_sr_callback_func[SR_CB_TYPE_CMD].args = args;
        break;

    case SR_CB_TYPE_CMD_EXIT:
        g_sr_callback_func[SR_CB_TYPE_CMD_EXIT].fn = handler;
        g_sr_callback_func[SR_CB_TYPE_CMD_EXIT].args = args;
        break;

    default:
        return ESP_ERR_INVALID_ARG;
        break;
    }

    return ESP_OK;
}

esp_err_t speech_recognition_init(void)
{
    xTaskCreatePinnedToCore(recsrcTask, "recsrcTask", 8 * 1024, NULL, 9, NULL, 1);

    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 2,
        .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    audio_player_config_t config = {
        .mute_fn = audio_player_mute,
        .clk_set_fn = audio_player_reconfig_std,
        .write_fn = audio_player_write,
    };
    ESP_ERROR_CHECK(audio_player_new(config));
    ESP_LOGI(TAG, "audio player init success");

    sr_handler_install(SR_CB_TYPE_WAKE, sr_wake, NULL);
    sr_handler_install(SR_CB_TYPE_CMD, sr_cmd, NULL);
    sr_handler_install(SR_CB_TYPE_CMD_EXIT, sr_cmd_exit, NULL);

    return ESP_OK;
}
