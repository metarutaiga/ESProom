/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/apps/sntp.h>

#include <esp_log.h>

#include "mod_sntp.h"

static const char * const TAG = "SNTP";

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "tw.pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();

    // wait for time to be set
    const int retry_count = 10;
    for (int retry = 1; retry <= retry_count; ++retry) {

        time_t now = 0;
        struct tm timeinfo = { 0 };

        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        break;
    }
}

static void sntp(void *parameter)
{
    setenv("TZ", "GMT-8", 1);
    tzset();

    for (;;) {

        time_t now = 0;
        struct tm timeinfo = { 0 };

        // update 'now' variable with current time
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGE(TAG, "The current date/time error");

            obtain_time();
        } else {
            char strftime_buf[64];
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            ESP_LOGI(TAG, "The current date/time in Taipei is: %s", strftime_buf);
            sntp_stop();

            break;
        }
    }

    vTaskDelete(NULL);
}

void mod_sntp(void)
{
    xTaskCreate(sntp, "sntp", 2048, NULL, 10, NULL);
}
