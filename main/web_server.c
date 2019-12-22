/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <esp_spi_flash.h>

#include "app_wifi.h"
#include "watt_hour_meter.h"
#include "web_server.h"

static const char * const TAG = "WEB-SERVER";

/* An HTTP GET handler */
esp_err_t home_get_handler(httpd_req_t *req)
{
    char *HTML = malloc(16384);
    char *html = HTML;

    time_t now = 0;
    struct tm timeinfo = { 0 };
    esp_chip_info_t info;
    uint8_t protocol_bitmap;
    wifi_bandwidth_t bw;
    uint8_t primary;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    wifi_country_t country;

    if (HTML == NULL)
        return ESP_OK;

    time(&now);
    localtime_r(&now, &timeinfo);

    // Head
    html += sprintf(html,
                    "<!DOCTYPE html>"
                    "<html>"
                    "<head>"
                    "<title>%s : Watt-Hour Meter</title>"
                    "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.0/Chart.min.js\"></script>"
                    "<meta http-equiv=\"refresh\" content=\"60\"/>"
                    "</head>"
                    "<body>", AREA_NAME);

    // Area
    html += sprintf(html, "<h1>%s</h1>", AREA_NAME);

    // Date Time
    html += sprintf(html, "<p>%d.%d.%d %d:%d:%d</p>", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Chart
    html += sprintf(html, "%s", "<canvas id=\"meter\" height=\"50%\"></canvas>");
    html += sprintf(html, "<script>");
    html += sprintf(html, "var ctx = document.getElementById('meter');");
    html += sprintf(html, "var myChart = new Chart(ctx, {");
    html += sprintf(html,   "type: 'line',");
    html += sprintf(html,   "data: {");
    html += sprintf(html,     "labels: [");
    for (int hour = 0; hour < 24; ++hour) {
        html += sprintf(html, "%s'%d'", hour ? "," : "", hour);
    }
    html += sprintf(html,     "],");
    html += sprintf(html,     "datasets: [{");
    html += sprintf(html,       "label: 'pulse',");
    html += sprintf(html,       "data: [");
    for (int hour = 0; hour < 24; ++hour) {
        html += sprintf(html, "%s%d", hour ? "," : "", PULSE_PER_HOUR[timeinfo.tm_mday][hour]);
    }
    html += sprintf(html,       "],");
    html += sprintf(html,       "borderWidth: 1");
    html += sprintf(html,     "}]");
    html += sprintf(html,   "}");
    html += sprintf(html, "});");
    html += sprintf(html, "</script>");

    // Table
    html += sprintf(html, "%s", "<table style=\"width:100%\" border='1'>");
    html += sprintf(html, "<tr>");
    html += sprintf(html, "<th>Day</th>");
    for (int hour = 0; hour < 24; ++hour) {
        html += sprintf(html, "<th>%02d</th>", hour);
    }
    html += sprintf(html, "<th>Total</th>");
    html += sprintf(html, "<th>kWh</th>");
    html += sprintf(html, "</tr>");
    for (int day = 1; day < 32; ++day) {
        int total = 0;

        html += sprintf(html, "<tr>");
        html += sprintf(html, "<th>%d</th>", day);
        for (int hour = 0; hour < 24; ++hour) {
            html += sprintf(html, "<th>%d</th>", PULSE_PER_HOUR[day][hour]);
            total += PULSE_PER_HOUR[day][hour];
        }
        html += sprintf(html, "<th>%d</th>", total);
        html += sprintf(html, "<th>%.2f</th>", total / (float)CONFIG_IMP_KWH);
        html += sprintf(html, "</tr>");
    }
    html += sprintf(html, "</table>");

    // Build
    html += sprintf(html, "<p>Build : %s %s</p>", __DATE__, __TIME__);

    // Log
    html += sprintf(html, "<p>");
    for (int i = 0; i < 8; ++i) {
        html += sprintf(html, "Log : %s<br>", LOG_BUFFER[(LOG_INDEX + i) % 8]);
    }
    html += sprintf(html, "</p>");

    // Heap
    html += sprintf(html, "<p>Free Heap Size : %u</p>", esp_get_free_heap_size());

    // Chip
    esp_chip_info(&info);
    html += sprintf(html, "<p>");
    html += sprintf(html, "Model : %s<br>", info.model == CHIP_ESP8266 ? "ESP8266" : info.model == CHIP_ESP32 ? "ESP32" : "Unknown");
    html += sprintf(html, "Feature : %s%s%s<br>",
                    info.features & CHIP_FEATURE_WIFI_BGN ? "802.11bgn" : "",
                    info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
                    info.features & CHIP_FEATURE_BT ? "/BT" : "");
    html += sprintf(html, "Cores : %d<br>", info.cores);
    html += sprintf(html, "Revision : %d<br>", info.revision);
    html += sprintf(html, "Flash : %dMB (%s)<br>",
                    spi_flash_get_chip_size() / (1024 * 1024),
                    info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded-Flash" : "External-Flash");
    html += sprintf(html, "IDF Version : %s<br>", esp_get_idf_version());
    html += sprintf(html, "</p>");

    // Wifi
    esp_wifi_get_protocol(ESP_IF_WIFI_STA, &protocol_bitmap);
    esp_wifi_get_bandwidth(ESP_IF_WIFI_STA, &bw);
    esp_wifi_get_channel(&primary, &second);
    esp_wifi_get_country(&country);
    html += sprintf(html, "<p>");
    html += sprintf(html, "Protocol : 802.11%s%s%s<br>", 
                    (protocol_bitmap & WIFI_PROTOCAL_11B) ? "b" : "",
                    (protocol_bitmap & WIFI_PROTOCAL_11G) ? "g" : "",
                    (protocol_bitmap & WIFI_PROTOCAL_11N) ? "n" : "");
    html += sprintf(html, "Bandwidth : %s<br>", 
                    (bw == WIFI_BW_HT20) ? "HT20" :
                    (bw == WIFI_BW_HT40) ? "HT40" : "Unknown");
    html += sprintf(html, "Channel : %d%s<br>", primary,
                    (second == WIFI_SECOND_CHAN_NONE) ? "" :
                    (second == WIFI_SECOND_CHAN_ABOVE) ? " (Above)" :
                    (second == WIFI_SECOND_CHAN_BELOW) ? " (Below)" : " (Unknown)");
    html += sprintf(html, "Country Code : %s<br>", country.cc);
    html += sprintf(html, "Country Start Channel : %d<br>", country.schan);
    html += sprintf(html, "Country Totol Channel : %d<br>", country.nchan);
    html += sprintf(html, "Country Max TX Power : %d<br>", country.max_tx_power);
    html += sprintf(html, "Country Policy : %s<br>", 
                    (country.policy == WIFI_COUNTRY_POLICY_AUTO) ? "Auto" :
                    (country.policy == WIFI_COUNTRY_POLICY_MANUAL) ? "Manual" : "Unknown");
    html += sprintf(html, "Restart Count : %d<br>", app_wifi_restart_count());
    html += sprintf(html, "</p>");

    // Tail
    html += sprintf(html,
                    "</body>"
                    "</html>");

    // Send response
    httpd_resp_send(req, HTML, strlen(HTML));

    free(HTML);

    return ESP_OK;
}

httpd_uri_t home = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = home_get_handler,
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &home);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}
