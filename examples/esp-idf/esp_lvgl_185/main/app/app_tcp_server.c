/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "app_tcp_server.h"

#include "app_wifi.h"


#define PORT         3333

static const char *TAG = "tcp_server";

static bool ir_learn_done = false;

static bool leaning_on_switch = true;
static bool leaning_off_switch = false;
static bool learning_on_send = false;
static bool learning_off_send = false;

static const char *payload_on = "OK: learning on";
static const char *payload_off = "OK: learning off";

static void udp_server_task(void *pvParameters)
{
    char* rx_buffer = NULL;
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr dest_addr;

    leaning_on_switch = true;

    while (1) {
        while(!app_wifi_is_connected()) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "wait Wi-Fi connected");
        }

        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGD(TAG, "Socket created");

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGD(TAG, "Socket bound, port %d", PORT);

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);

        while (1) {
            ESP_LOGI(TAG, "Waiting for data");
            
            rx_buffer = (char *)heap_caps_calloc(1, 1024, MALLOC_CAP_SPIRAM);
            int len = recvfrom(sock, rx_buffer, 1023, 0, (struct sockaddr *)&source_addr, &socklen);
            
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGD(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.ss_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                } 

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                
                if(leaning_on_switch) {
                    FILE *file = fopen("/spiffs/my_learn_on.cfg", "w");
                    if (file == NULL) {
                        ESP_LOGE(TAG, "Failed to open file for writing");
                    }

                    size_t bytes_written = fwrite(rx_buffer, 1, len, file);
                    if (bytes_written != len) {
                        ESP_LOGE(TAG, "Write operation failed");
                    } else {
                        ESP_LOGW(TAG, "Write operation was successful");
                        learning_on_send = true;
                    }
                    fclose(file);
                } else if(leaning_off_switch) {
                    FILE *file = fopen("/spiffs/my_learn_off.cfg", "w");
                    if (file == NULL) {
                        ESP_LOGE(TAG, "Failed to open file for writing");
                    }

                    size_t bytes_written = fwrite(rx_buffer, 1, len, file);
                    if (bytes_written != len) {
                        ESP_LOGE(TAG, "Write operation failed");
                    } else {
                        ESP_LOGW(TAG, "Write operation was successful");
                        learning_off_send = true;

                        if(!ir_learn_done) {
                            // ir_start();
                            ir_learn_done = true;
                        } else {
                            // ir_stop();
                            // ir_start();
                        }
                        ESP_LOGW(TAG, "ir_learn_done");
                    }
                    fclose(file);
                }
            }

            if(learning_on_send) {
                int err = sendto(sock, payload_on, strlen(payload_on), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                ESP_LOGW(TAG, "payload on sent");

                leaning_on_switch = false;
                leaning_off_switch = true;

                learning_on_send = false;
            } else if(learning_off_send) {
                int err = sendto(sock, payload_off, strlen(payload_off), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
                ESP_LOGW(TAG, "payload off sent");

                leaning_off_switch = false;
                leaning_on_switch = true;

                learning_off_send = false;
            }

            heap_caps_free(rx_buffer);

        }
        if (sock != -1) {
            ESP_LOGD(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }

    ESP_LOGE(TAG, "app udp server task is exiting");
    vTaskDelete(NULL);
}

void app_udp_server_init(void)
{
    xTaskCreate(udp_server_task, "udp_server", 3072, (void*)AF_INET, 5, NULL);
}
