/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <mqtt_client.h>

#include "mod_watt_hour_meter.h"
#include "mod_mqtt.h"

static char MQTT_INIT;
static char MQTT_DATA;
static char MQTT_NAME[32];
static esp_mqtt_client_handle_t MQTT_CLIENT;

static const char * const TAG = "MQTT";

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    char topic[64];
    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            msg_id = esp_mqtt_client_subscribe(client, MQTT_NAME, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, MQTT_NAME);
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

            MQTT_CLIENT = client;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            MQTT_CLIENT = NULL;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

            MQTT_INIT = 0;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);

            sprintf(topic, "%s/connected", MQTT_NAME);
            msg_id = esp_mqtt_client_publish(client, topic, "1", 0, 0, 1);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            sprintf(topic, "%s/build", MQTT_NAME);
            msg_id = esp_mqtt_client_publish(client, topic, __DATE__ " " __TIME__, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            sprintf(topic, "%s/name", MQTT_NAME);
            msg_id = esp_mqtt_client_publish(client, topic, (char*)AREA_NAME, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            MQTT_INIT = 1;
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            
            if (event->data_len != 0 && MQTT_DATA == 0) {
                char *data = malloc(event->data_len + 1);
                if (data) {
                    memcpy(data, event->data, event->data_len);
                    data[event->data_len] = 0;

                    char *json_day = strstr(data, "\"day\":");
                    char *json_values = strstr(data, "\"values\":[");
                    if (json_day && json_values) {
                        json_values += sizeof("\"values\":[") - 1;
                        json_day += sizeof("\"day\":") - 1;

                        char *token = NULL;
                        char *step = strtok_r(json_values, ",", &token);
                        if (step) {

                            time_t now = 0;
                            struct tm timeinfo = { 0 };

                            time(&now);
                            localtime_r(&now, &timeinfo);

                            int day = atoi(json_day);
                            if (day == timeinfo.tm_mday) {
                                for (int i = 0; i < 24; ++i) {
                                    PULSE_PER_HOUR[day][i] += atoi(step);
                                    step = strtok_r(NULL, ",", &token);
                                    if (step == NULL)
                                        break;
                                }
                            }
                        }
                    }
                    free(data);
                }
            }

            MQTT_DATA = 1;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

void mod_mqtt_publish(void)
{
    if (MQTT_CLIENT == 0 || MQTT_INIT == 0)
        return;

    time_t now = 0;
    struct tm timeinfo = { 0 };

    time(&now);
    localtime_r(&now, &timeinfo);

    char data[256];
    sprintf(data, "{"
                  "\"day\":%d,"
                  "\"power\":%.2f,"
                  "\"values\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]"
                  "}", timeinfo.tm_mday,
                       (60.0 * 60.0 * 1000.0 * 1000.0 * 1000.0) / (CURRENT_TIME - PREVIOUS_TIME) / CONFIG_IMP_KWH,
                       PULSE_PER_HOUR[timeinfo.tm_mday][0],
                       PULSE_PER_HOUR[timeinfo.tm_mday][1],
                       PULSE_PER_HOUR[timeinfo.tm_mday][2],
                       PULSE_PER_HOUR[timeinfo.tm_mday][3],
                       PULSE_PER_HOUR[timeinfo.tm_mday][4],
                       PULSE_PER_HOUR[timeinfo.tm_mday][5],
                       PULSE_PER_HOUR[timeinfo.tm_mday][6],
                       PULSE_PER_HOUR[timeinfo.tm_mday][7],
                       PULSE_PER_HOUR[timeinfo.tm_mday][8],
                       PULSE_PER_HOUR[timeinfo.tm_mday][9],
                       PULSE_PER_HOUR[timeinfo.tm_mday][10],
                       PULSE_PER_HOUR[timeinfo.tm_mday][11],
                       PULSE_PER_HOUR[timeinfo.tm_mday][12],
                       PULSE_PER_HOUR[timeinfo.tm_mday][13],
                       PULSE_PER_HOUR[timeinfo.tm_mday][14],
                       PULSE_PER_HOUR[timeinfo.tm_mday][15],
                       PULSE_PER_HOUR[timeinfo.tm_mday][16],
                       PULSE_PER_HOUR[timeinfo.tm_mday][17],
                       PULSE_PER_HOUR[timeinfo.tm_mday][18],
                       PULSE_PER_HOUR[timeinfo.tm_mday][19],
                       PULSE_PER_HOUR[timeinfo.tm_mday][20],
                       PULSE_PER_HOUR[timeinfo.tm_mday][21],
                       PULSE_PER_HOUR[timeinfo.tm_mday][22],
                       PULSE_PER_HOUR[timeinfo.tm_mday][23]);
    esp_mqtt_client_publish(MQTT_CLIENT, MQTT_NAME, data, 0, 0, 1);
}

void mod_mqtt(void)
{
    char topic[64];
    sprintf(topic, "%s/connected", MQTT_NAME);

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
        .event_handle = mqtt_event_handler,
        .lwt_topic = topic,
        .lwt_msg = "0",
        .lwt_retain = 1,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    const char* hostname = "";
    tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &hostname);
    strcpy(MQTT_NAME, hostname);
}
