/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <mqtt_client.h>

#include "mod_web_server.h"
#include "mod_watt_hour_meter.h"

unsigned short PULSE_PER_HOUR[32][24];
unsigned char AREA_NAME[16];
int64_t CURRENT_TIME;
int64_t PREVIOUS_TIME;

static struct tm PULSE_TIMEINFO;
static char MQTT_INIT;
static char MQTT_DATA;
static char MQTT_NAME[32];
static esp_mqtt_client_handle_t MQTT_CLIENT;

static const char * const CONFIG_FORM_HOUR[24] =
{
    CONFIG_FORM_HOUR_00,
    CONFIG_FORM_HOUR_01,
    CONFIG_FORM_HOUR_02,
    CONFIG_FORM_HOUR_03,
    CONFIG_FORM_HOUR_04,
    CONFIG_FORM_HOUR_05,
    CONFIG_FORM_HOUR_06,
    CONFIG_FORM_HOUR_07,
    CONFIG_FORM_HOUR_08,
    CONFIG_FORM_HOUR_09,
    CONFIG_FORM_HOUR_10,
    CONFIG_FORM_HOUR_11,
    CONFIG_FORM_HOUR_12,
    CONFIG_FORM_HOUR_13,
    CONFIG_FORM_HOUR_14,
    CONFIG_FORM_HOUR_15,
    CONFIG_FORM_HOUR_16,
    CONFIG_FORM_HOUR_17,
    CONFIG_FORM_HOUR_18,
    CONFIG_FORM_HOUR_19,
    CONFIG_FORM_HOUR_20,
    CONFIG_FORM_HOUR_21,
    CONFIG_FORM_HOUR_22,
    CONFIG_FORM_HOUR_23,
};

static const char * const TAG = "WATT-HOUR METER";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

static void pulse_web(void *parameter)
{
    char* HTTP_URL = malloc(1024);
    char* http_url = HTTP_URL;
    esp_http_client_config_t config = {
        .url = HTTP_URL,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client;
    esp_err_t err;

    int total = 0;

    time_t now = 0;
    struct tm timeinfo = { 0 };

    if (HTTP_URL == NULL)
        return;

    // URL
    http_url += sprintf(http_url, CONFIG_FORM_URL "/forms/d/e/");
    http_url += sprintf(http_url, CONFIG_FORM_ID);
    http_url += sprintf(http_url, "/formResponse?usp=pp_url&");

    // Area
    http_url += sprintf(http_url, "%s%s&", CONFIG_FORM_AREA, AREA_NAME);
   
    // Pulse
    for (int i = 0; i < 24; ++i) {
        total += PULSE_PER_HOUR[PULSE_TIMEINFO.tm_mday][i];
        http_url += sprintf(http_url, "%s%d&", CONFIG_FORM_HOUR[i], PULSE_PER_HOUR[PULSE_TIMEINFO.tm_mday][i]);
    }

    // Total
    http_url += sprintf(http_url, "%s%d&", CONFIG_FORM_TOTAL, total);

    // Submit
    http_url += sprintf(http_url, "submit=Submit");
    ESP_LOGI(TAG, "%s", HTTP_URL);

    client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %d", err);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Time
    time(&now);
    localtime_r(&now, &timeinfo);
    PULSE_TIMEINFO = timeinfo;

    free(HTTP_URL);
    
    vTaskDelete(NULL);
}

static void pulse_log(void *parameter)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (MQTT_CLIENT && MQTT_INIT) {
        char data[256];

        sprintf(data, "{"
                      "\"day\":%d,"
                      "\"power\":%.2f,"
                      "\"values\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]"
                      "}",
                timeinfo.tm_mday,
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
#if WATT_DEBUG
    ESP_LOGI(TAG, "pulse : %d (%d.%d.%d %d:%d:%d)", PULSE_PER_HOUR[timeinfo.tm_mday][timeinfo.tm_hour], timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
#endif
    vTaskDelete(NULL);
}

static void pulse(void *parameter)
{
    static int last_hour = -1;
    static int64_t last_time = 0;
    int64_t pulse_time = esp_timer_get_time();
    int64_t delta_time = pulse_time - last_time;
    time_t now;
    struct tm timeinfo;

    if (delta_time < 100 * 1000)
       return;
    last_time = pulse_time;

    PREVIOUS_TIME = CURRENT_TIME;
    CURRENT_TIME = pulse_time;

    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900))
        return;

    if (PULSE_TIMEINFO.tm_year < (2016 - 1900))
        PULSE_TIMEINFO = timeinfo;

    PULSE_PER_HOUR[timeinfo.tm_mday][timeinfo.tm_hour]++;
    xTaskCreate(&pulse_log, "pulse_log", 2048, NULL, 5, NULL);

    if (last_hour == timeinfo.tm_hour)
        return;
    if (last_hour != -1) {
        xTaskCreate(&pulse_web, "pulse_web", 4096, NULL, 5, NULL);
        if (last_hour == 23) {
            for (int i = 0; i < 24; ++i)
                PULSE_PER_HOUR[timeinfo.tm_mday][i] = 0;
            PULSE_PER_HOUR[timeinfo.tm_mday][timeinfo.tm_hour]++;
        }
    }
    last_hour = timeinfo.tm_hour;
}

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
                char* data = malloc(event->data_len + 1);
                if (data) {
                    time_t now;
                    struct tm timeinfo;

                    char* json_day = NULL;
                    char* json_values = NULL;

                    time(&now);
                    localtime_r(&now, &timeinfo);

                    memcpy(data, event->data, event->data_len);
                    data[event->data_len] = 0;

                    json_day = strstr(data, "\"day\":");
                    json_values = strstr(data, "\"values\":[");
                    if (json_day && json_values) {
                        char* token = NULL;
                        char* step = strtok_r(json_values + sizeof("\"values\":[") - 1, ",", &token);
                        if (step) {
                            int day = atoi(json_day + sizeof("\"day\":") - 1);
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

static void mqtt_app_start(void)
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
}

void mod_watt_hour_meter(gpio_num_t gpio_num)
{
    // Get MAC
    uint8_t mac[6] = { 0 };
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    // Set Hostname
    char hostname[16];
    sprintf(hostname, "WATT_%02X%02X%02X", mac[3], mac[4], mac[5]);    
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);

    // Set Area
    char *config_area = strstr(CONFIG_AREA, hostname + sizeof("WATT_") - 1);
    if (config_area != NULL) {
        config_area = strchr(config_area, '=');
        if (config_area != NULL) {
            config_area++;
        }
    }
    if (config_area == NULL) {
        config_area = CONFIG_AREA;
    }
    for (int i = 0; i < sizeof(AREA_NAME); ++i) {
        char c = config_area[i];
        if (c == ';')
            c = 0;
        AREA_NAME[i] = c;
        if (c == 0)
            break;
    }
    AREA_NAME[sizeof(AREA_NAME) - 1] = 0;

    strcpy(MQTT_NAME, hostname);
    mqtt_app_start();

    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
    gpio_set_intr_type(gpio_num, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_num, pulse, NULL);
}

void mod_watt_hour_meter_http_handler(httpd_req_t *req)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };

    time(&now);
    localtime_r(&now, &timeinfo);

    // Chart
    mod_webserver_printf(req, "%s", "<canvas id=\"meter\" height=\"50%\"></canvas>");
    mod_webserver_printf(req, "<script>");
    mod_webserver_printf(req, "var ctx = document.getElementById('meter');");
    mod_webserver_printf(req, "var myChart = new Chart(ctx, {");
    mod_webserver_printf(req,   "type: 'line',");
    mod_webserver_printf(req,   "data: {");
    mod_webserver_printf(req,     "labels: [");
    for (int hour = 0; hour < 24; hour += 6) {
        mod_webserver_printf(req, "%s'%d'"
                                  "%s'%d'"
                                  "%s'%d'"
                                  "%s'%d'"
                                  "%s'%d'"
                                  "%s'%d'", (hour + 0) ? "," : "", (hour + 0),
                                            (hour + 1) ? "," : "", (hour + 1),
                                            (hour + 2) ? "," : "", (hour + 2),
                                            (hour + 3) ? "," : "", (hour + 3),
                                            (hour + 4) ? "," : "", (hour + 4),
                                            (hour + 5) ? "," : "", (hour + 5));
    }
    mod_webserver_printf(req,     "],");
    mod_webserver_printf(req,     "datasets: [{");
    mod_webserver_printf(req,       "label: 'pulse',");
    mod_webserver_printf(req,       "data: [");
    for (int hour = 0; hour < 24; hour += 6) {
        mod_webserver_printf(req, "%s%d"
                                  "%s%d"
                                  "%s%d"
                                  "%s%d"
                                  "%s%d"
                                  "%s%d", (hour + 0) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 0],
                                          (hour + 1) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 1],
                                          (hour + 2) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 2],
                                          (hour + 3) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 3],
                                          (hour + 4) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 4],
                                          (hour + 5) ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour + 5]);
    }
    mod_webserver_printf(req,       "],");
    mod_webserver_printf(req,       "borderWidth: 1");
    mod_webserver_printf(req,     "}]");
    mod_webserver_printf(req,   "}");
    mod_webserver_printf(req, "});");
    mod_webserver_printf(req, "</script>");

    // Table
    mod_webserver_printf(req, "%s", "<table style=\"width:100%\" border='1'>");
    mod_webserver_printf(req, "<tr>");
    mod_webserver_printf(req, "<th>Day</th>");
    for (int hour = 0; hour < 24; hour += 6) {
        mod_webserver_printf(req, "<th>%02d</th>"
                                  "<th>%02d</th>"
                                  "<th>%02d</th>"
                                  "<th>%02d</th>"
                                  "<th>%02d</th>"
                                  "<th>%02d</th>", (hour + 0),
                                                   (hour + 1),
                                                   (hour + 2),
                                                   (hour + 3),
                                                   (hour + 4),
                                                   (hour + 5));
    }
    mod_webserver_printf(req, "<th>Total</th>");
    mod_webserver_printf(req, "<th>kWh</th>");
    mod_webserver_printf(req, "</tr>");
    for (int day = 1; day < 32; ++day) {
        int total = 0;

        mod_webserver_printf(req, "<tr>");
        mod_webserver_printf(req, "<th>%d</th>", day);
        for (int hour = 0; hour < 24; hour += 6) {
            mod_webserver_printf(req, "<th>%d</th>"
                                      "<th>%d</th>"
                                      "<th>%d</th>"
                                      "<th>%d</th>"
                                      "<th>%d</th>"
                                      "<th>%d</th>", PULSE_PER_HOUR[day][hour + 0],
                                                     PULSE_PER_HOUR[day][hour + 1],
                                                     PULSE_PER_HOUR[day][hour + 2],
                                                     PULSE_PER_HOUR[day][hour + 3],
                                                     PULSE_PER_HOUR[day][hour + 4],
                                                     PULSE_PER_HOUR[day][hour + 5]);
            total += PULSE_PER_HOUR[day][hour];
        }
        mod_webserver_printf(req, "<th>%d</th>", total);
        mod_webserver_printf(req, "<th>%.2f</th>", total / (float)CONFIG_IMP_KWH);
        mod_webserver_printf(req, "</tr>");
    }
    mod_webserver_printf(req, "</table>");
}
