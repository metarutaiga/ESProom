/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "lwip/apps/sntp.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "app_wifi.h"

#include "esp_http_client.h"

static int PULSE_PER_HOUR[24];
static char HTTP_URL[1024];
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

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "tw.pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    app_wifi_wait_connected();
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void sntp(void *parameter)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    while (1) {

        // update 'now' variable with current time
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGE(TAG, "The current date/time error");

            obtain_time();
            setenv("TZ", "GMT-8", 1);
            tzset();
        } else {
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "The current date/time in Taipei is: %s", strftime_buf);
            break;
        }
    }

    vTaskDelete(NULL);
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
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
    esp_http_client_config_t config = {
        .url = HTTP_URL,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client;
    esp_err_t err;

    char temp[64];
    int total = 0;

    // URL
    strcpy(HTTP_URL, "https://docs.google.com/forms/d/e/");
    strcat(HTTP_URL, CONFIG_FORM_ID);
    strcat(HTTP_URL, "/formResponse?usp=pp_url&");

    // Area
    sprintf(temp, "%s%s&", CONFIG_FORM_AREA, CONFIG_AREA);
    strcat(HTTP_URL, temp);
   
    // Pulse
    for (int i = 0; i < 24; ++i) {
        total += PULSE_PER_HOUR[i];
        sprintf(temp, "%s%d&", CONFIG_FORM_HOUR[i], PULSE_PER_HOUR[i]);
        strcat(HTTP_URL, temp);      
    }

    // Total
    sprintf(temp, "%s%d&", CONFIG_FORM_TOTAL, total);
    strcat(HTTP_URL, temp);

    // Submit
    strcat(HTTP_URL, "submit=Submit");
    ESP_LOGI(TAG, "%s", HTTP_URL);

    app_wifi_wait_connected();
    client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %d", err);
    }
    esp_http_client_cleanup(client);
    
    vTaskDelete(NULL);
}

static void pulse_log(void *parameter)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "pulse : %d (%d:%d:%d)", PULSE_PER_HOUR[timeinfo.tm_hour], timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
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

    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900))
        return;

    PULSE_PER_HOUR[timeinfo.tm_hour]++;
    xTaskCreate(&pulse_log, "pulse_log", 8192, NULL, 5, NULL);

    if (last_hour == timeinfo.tm_hour)
        return;
    if (last_hour != -1) {
        xTaskCreate(&pulse_web, "pulse_web", 8192, NULL, 5, NULL);
        if (last_hour == 23) {
            for (int i = 0; i < 24; ++i)
                PULSE_PER_HOUR[i] = 0;
            PULSE_PER_HOUR[timeinfo.tm_hour]++;
        }
    }
    last_hour = timeinfo.tm_hour;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    app_wifi_initialise();

    xTaskCreate(sntp, "sntp", 2048, NULL, 10, NULL);

    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);
    gpio_set_intr_type(GPIO_NUM_2, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_2, pulse, NULL);
}
