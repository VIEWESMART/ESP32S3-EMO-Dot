// /* ESPNOW Example

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */

// /*
//    This example shows how to use ESPNOW.
//    Prepare two device, one for sending ESPNOW data and another for receiving
//    ESPNOW data.
// */
// #include <stdlib.h>
// #include <time.h>
// #include <string.h>
// #include <assert.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"
// #include "freertos/timers.h"
// #include "nvs_flash.h"
// #include "esp_random.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "esp_wifi.h"
// #include "esp_log.h"
// #include "esp_mac.h"
// #include "esp_now.h"
// #include "esp_crc.h"
// #include "app_espnow.h"
// #include "app_icm.h"
// #define ESPNOW_MAXDELAY 512

// static const char *TAG = "app_espnow";
// #define CONFIG_ESPNOW_PMK "pmk1234567890123"
// #define CONFIG_ESPNOW_LMK "lmk1234567890123"
// #define CONFIG_ESPNOW_SEND_COUNT 100
// #define CONFIG_ESPNOW_SEND_DELAY 1000
// #define CONFIG_ESPNOW_SEND_LEN 18
// static QueueHandle_t s_example_espnow_queue;
// extern QueueHandle_t icm_espnow_send_queue;
// extern QueueHandle_t icm_espnow_receive_queue;
// static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
// static uint8_t s_example_ignore_mac[ESP_NOW_ETH_ALEN] = { 0xdc,0x36, 0x43, 0xb0, 0x19, 0x73 };
// static uint16_t s_example_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };
// static int8_t pair_ok =0;
// static void example_espnow_deinit(example_espnow_send_param_t *send_param);

// /* WiFi should start before using ESPNOW */
// static void example_wifi_init(void)
// {
    
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
//     ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
//     ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
//     ESP_ERROR_CHECK( esp_wifi_start());
//     ESP_ERROR_CHECK( esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

// #if CONFIG_ESPNOW_ENABLE_LONG_RANGE
//     ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
// #endif
// }

// /* ESPNOW sending or receiving callback function is called in WiFi task.
//  * Users should not do lengthy operations from this task. Instead, post
//  * necessary data to a queue and handle it from a lower priority task. */
// static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
// {
//     example_espnow_event_t evt;
//     example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

//     if (mac_addr == NULL) {
//         ESP_LOGE(TAG, "Send cb arg error");
//         return;
//     }

//     evt.id = ESPNOW_SEND_CB;
//     memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
//     send_cb->status = status;
//     if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
//         ESP_LOGW(TAG, "Send send queue fail");
//     }
//     // ESP_LOGI(TAG, "espnow_send_cb ");
// }

// static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
// {
//     example_espnow_event_t evt;
//     example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
//     uint8_t * mac_addr = recv_info->src_addr;

//     if (mac_addr == NULL || data == NULL || len <= 0) {
//         ESP_LOGE(TAG, "Receive cb arg error");
//         return;
//     }

//     evt.id = ESPNOW_RECV_CB;
//     memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
//     recv_cb->data = malloc(len);
//     if (recv_cb->data == NULL) {
//         ESP_LOGE(TAG, "Malloc receive data fail");
//         return;
//     }
//     memcpy(recv_cb->data, data, len);
//     recv_cb->data_len = len;
//     if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
//         ESP_LOGW(TAG, "Send receive queue fail");
//         free(recv_cb->data);
//     }
// }

// /* Parse received ESPNOW data. */
// int example_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint8_t *payload)
// {
//     example_espnow_data_t *buf = (example_espnow_data_t *)data;
//     uint16_t crc, crc_cal = 0;

//     if (data_len < sizeof(example_espnow_data_t)) {
//         ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
//         return -1;
//     }

//     *state = buf->state;
//     memcpy(payload, buf->payload, CONFIG_ESPNOW_SEND_LEN-4);
//     crc = buf->crc;
//     buf->crc = 0;
//     crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

//     if (crc_cal == crc) {
//         return buf->type;
//     }

//     return -1;
// }

// /* Prepare ESPNOW data to be sent. */
// void example_espnow_data_prepare(example_espnow_send_param_t *send_param)
// {
//     example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;
//     icm_espnow_quenedata_t icm_espnow_quenedata;
//     assert(send_param->len >= sizeof(example_espnow_data_t));
//     buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? EXAMPLE_ESPNOW_DATA_BROADCAST : EXAMPLE_ESPNOW_DATA_UNICAST;
//     buf->state = send_param->state;
//     buf->crc = 0;
//     if (pair_ok==1) {
//         if (xQueueReceive(icm_espnow_send_queue, &icm_espnow_quenedata, portMAX_DELAY) == pdTRUE) {
//             buf->payload[0]=(uint8_t)(icm_espnow_quenedata.ball_x & 0xff);  
//             buf->payload[1]=(uint8_t)((icm_espnow_quenedata.ball_x >> 8) & 0xff); 
//             buf->payload[2]=(uint8_t)(icm_espnow_quenedata.ball_y & 0xff);  
//             buf->payload[3]=(uint8_t)((icm_espnow_quenedata.ball_y >> 8) & 0xff);
//             buf->payload[4]=(uint8_t)(icm_espnow_quenedata.line_x & 0xff);
//             buf->payload[5]=(uint8_t)((icm_espnow_quenedata.line_x >> 8) & 0xff);
//             buf->payload[6]=(uint8_t)(icm_espnow_quenedata.line_y & 0xff);
//             buf->payload[7]=(uint8_t)((icm_espnow_quenedata.line_y >> 8) & 0xff);
//             buf->payload[8]=icm_espnow_quenedata.turn;
//             buf->payload[9]=icm_espnow_quenedata.ball_pos;
//             // ESP_LOGI(TAG,"icm_espnow_quenedata.ball_x=%d,icm_espnow_quenedata.ball_y=%d",icm_espnow_quenedata.ball_x,icm_espnow_quenedata.ball_y);
//         }
//     }
    
//     // ESP_LOGI(TAG, "payload: %d", send_param->len - sizeof(example_espnow_data_t));
//     buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
// }

// static void example_espnow_task(void *pvParameter)
// {
//     example_espnow_event_t evt;
//     uint8_t recv_state;
//     uint8_t payload[CONFIG_ESPNOW_SEND_LEN-4]={};
//     bool is_broadcast = false;
//     int ret=0;
//     bool master_status=0;
//     // vTaskDelay(5000 / portTICK_PERIOD_MS);
//     ESP_LOGI(TAG, "Start sending broadcast data");

//     /* Start sending broadcast ESPNOW data. */
//     example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;
//     if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
//         ESP_LOGE(TAG, "Send error");
//         example_espnow_deinit(send_param);
//         vTaskDelete(NULL);
//     }

//     while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
//         switch (evt.id) {
//             case ESPNOW_SEND_CB:
//             {
//                 example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
//                 is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);
//                 // ESP_LOGI(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

//                   vTaskDelay(pdMS_TO_TICKS(10));
//                 // memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
//                 example_espnow_data_prepare(send_param);

//                 /* Send the next data after the previous data is sent. */
//                 if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
//                     ESP_LOGE(TAG, "Send error");
//                     example_espnow_deinit(send_param);
//                     vTaskDelete(NULL);
//                 }
//                 // ESP_LOGI(TAG, "Send data finished");
//                 break;
//             }
//             case ESPNOW_RECV_CB:
//             {
//                 example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

//                 ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &payload[0]);
//                 icm_espnow_quenedata_t icm_espnow_quenedata;
//                 icm_espnow_quenedata.ball_x = (int16_t)(payload[1]<<8|payload[0]);
//                 icm_espnow_quenedata.ball_y = (int16_t)(payload[3]<<8|payload[2]);
//                 icm_espnow_quenedata.line_x = (int16_t)(payload[5]<<8|payload[4]);
//                 icm_espnow_quenedata.line_y = (int16_t)(payload[7]<<8|payload[6]);
//                 icm_espnow_quenedata.status = master_status;
//                 icm_espnow_quenedata.turn = payload[8];
//                 icm_espnow_quenedata.ball_pos = payload[9];
//                 // ESP_LOGW(TAG, "ret = %d,payload: %d,%d,%d,%d",ret,icm_espnow_quenedata.ball_x,icm_espnow_quenedata.ball_y,icm_espnow_quenedata.line_x,icm_espnow_quenedata.line_y);
//                 xQueueSend(icm_espnow_receive_queue, &icm_espnow_quenedata, 0);
//                 free(recv_cb->data);
//                 if (ret == EXAMPLE_ESPNOW_DATA_BROADCAST) {
//                     ESP_LOGI(TAG, "Receive broadcast data from: "MACSTR", len: %d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

//                     /* If MAC address does not exist in peer list, add it to peer list. */
//                     if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
//                         esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
//                         if (peer == NULL) {
//                             ESP_LOGE(TAG, "Malloc peer information fail");
//                             example_espnow_deinit(send_param);
//                             vTaskDelete(NULL);
//                         }
//                         memset(peer, 0, sizeof(esp_now_peer_info_t));
//                         peer->channel = 0;
//                         peer->ifidx = ESP_IF_WIFI_STA;
//                         peer->encrypt = true;
//                         memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
//                         memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
//                         ESP_ERROR_CHECK( esp_now_add_peer(peer) );
//                         free(peer);
//                     }
//                     if (recv_state == 23) {
//                         memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
//                         send_param->broadcast = false;
//                         send_param->unicast = true;
//                         pair_ok=1;
//                         uint8_t my_mac[6]; 
//                         esp_efuse_mac_get_default(my_mac); 
//                         master_status = memcmp(send_param->dest_mac, my_mac, ESP_NOW_ETH_ALEN)  >0 ? 1 : 0;
//                         // example_espnow_data_prepare(send_param);
//                         // esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len);
//                         // ESP_LOGI(TAG, "dest_mac = "MACSTR",my_mac = "MACSTR", master_status=%d",MAC2STR(send_param->dest_mac),MAC2STR(my_mac),master_status);
//                     }
//                 }
//                 else if (ret == EXAMPLE_ESPNOW_DATA_UNICAST) {
//                     if (recv_state == 23){
//                         memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
//                         send_param->broadcast = false;
//                         send_param->unicast = true;
//                         pair_ok=1;
//                         uint8_t my_mac[6]; 
//                         esp_efuse_mac_get_default(my_mac); 
//                         master_status = memcmp(send_param->dest_mac, my_mac, ESP_NOW_ETH_ALEN)  >0 ? 1 : 0;
//                     }
//                     send_param->broadcast = false;
//                 }
//                 else {
//                     if (memcmp(s_example_ignore_mac,recv_cb->mac_addr,ESP_NOW_ETH_ALEN) != 0) {
//                         ESP_LOGI(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
//                     }
                    
                    
//                 }
//                 break;
//             }
//             default:
//                 ESP_LOGE(TAG, "Callback type error: %d", evt.id);
//                 break;
//         }
//         // ESP_LOGI(TAG,"evt.id = %d,ret = %d",evt.id, ret);
//     }
// }

// esp_err_t app_espnow_init(void)
// {
//     example_wifi_init();
//     // ESP_ERROR_CHECK( esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
//     wifi_mode_t s_wifi_mode;
//     do {
//         int ret = esp_wifi_get_mode(&s_wifi_mode);
//         ESP_LOGI(TAG, "Get wifi mode: %d", s_wifi_mode);
//         if (ret != ESP_OK) {
//             ESP_LOGE(TAG, "Get wifi mode error");
//             s_wifi_mode = WIFI_MODE_NULL;
//         }
//         vTaskDelay(pdMS_TO_TICKS(100));
//     } while (s_wifi_mode == WIFI_MODE_NULL);
//     example_espnow_send_param_t *send_param;

//     s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
//     if (s_example_espnow_queue == NULL) {
//         ESP_LOGE(TAG, "Create mutex fail");
//         return ESP_FAIL;
//     }

//     /* Initialize ESPNOW and register sending and receiving callback function. */
//     ESP_ERROR_CHECK( esp_now_init() );
//     ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
//     ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );
// #if CONFIG_ESPNOW_ENABLE_POWER_SAVE
//     ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
//     ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
// #endif
//     /* Set primary master key. */
//     ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

//     /* Add broadcast peer information to peer list. */
//     esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
//     if (peer == NULL) {
//         ESP_LOGE(TAG, "Malloc peer information fail");
//         vSemaphoreDelete(s_example_espnow_queue);
//         esp_now_deinit();        return ESP_FAIL;
//     }
//     memset(peer, 0, sizeof(esp_now_peer_info_t));
//     peer->channel = 0;
//     peer->ifidx = ESP_IF_WIFI_STA;
//     peer->encrypt = false;
//     memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
//     ESP_ERROR_CHECK( esp_now_add_peer(peer) );
//     free(peer);

//     /* Initialize sending parameters. */
//     send_param = malloc(sizeof(example_espnow_send_param_t));
//     if (send_param == NULL) {
//         ESP_LOGE(TAG, "Malloc send parameter fail");
//         vSemaphoreDelete(s_example_espnow_queue);
//         esp_now_deinit();
//         return ESP_FAIL;
//     }
//     memset(send_param, 0, sizeof(example_espnow_send_param_t));
//     send_param->unicast = false;
//     send_param->broadcast = true;
//     send_param->state = 23;
//     send_param->len = CONFIG_ESPNOW_SEND_LEN;
//     send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
//     if (send_param->buffer == NULL) {
//         ESP_LOGE(TAG, "Malloc send buffer fail");
//         free(send_param);
//         vSemaphoreDelete(s_example_espnow_queue);
//         esp_now_deinit();
//         return ESP_FAIL;
//     }
//     memcpy(send_param->dest_mac, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
//     example_espnow_data_prepare(send_param);

//     xTaskCreate(example_espnow_task, "example_espnow_task", 4096, send_param, 4, NULL);

//     return ESP_OK;
// }

// static void example_espnow_deinit(example_espnow_send_param_t *send_param)
// {
//     free(send_param->buffer);
//     free(send_param);
//     vSemaphoreDelete(s_example_espnow_queue);
//     esp_now_deinit();
// }

 