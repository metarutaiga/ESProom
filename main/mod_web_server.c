/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_spi_flash.h>

#include "mod_bme680.h"
#include "mod_log.h"
#include "mod_watt_hour_meter.h"
#include "mod_web_server.h"
#include "mod_wifi.h"

static const char * const TAG = "WEB-SERVER";

/* An HTTP GET handler */
static esp_err_t home_get_handler(httpd_req_t *req)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };

    time(&now);
    localtime_r(&now, &timeinfo);

    // Head
    mod_webserver_printf(req, "<!DOCTYPE html>");
    mod_webserver_printf(req, "<html>");
    mod_webserver_printf(req, "<head>");
    mod_webserver_printf(req, "<title>%s : Watt-Hour Meter</title>", AREA_NAME);
    mod_webserver_printf(req, "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.0/Chart.min.js\"></script>");
    mod_webserver_printf(req, "<meta http-equiv=\"refresh\" content=\"60\"/>");
    mod_webserver_printf(req, "</head>");
    mod_webserver_printf(req, "<body>");

    // Area
    mod_webserver_printf(req, "<h1>%s - %.2fW</h1>", AREA_NAME, (60.0 * 60.0 * 1000.0 * 1000.0 * 1000.0) / (CURRENT_TIME - PREVIOUS_TIME) / CONFIG_IMP_KWH);

    // Date Time
    mod_webserver_printf(req, "<p>");
    mod_webserver_printf(req, "Clock : %d.%d.%d %d:%d:%d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    mod_webserver_printf(req, "<br>");
    mod_webserver_printf(req, "Build : %s %s", __DATE__, __TIME__);
    mod_webserver_printf(req, "</p>");

    // Modules
    mod_watt_hour_meter_http_handler(req);
    mod_bme680_http_handler(req);
    mod_log_http_handler(req);
    mod_wifi_http_handler(req);

    // Chip
    esp_chip_info_t info;
    esp_chip_info(&info);
    mod_webserver_printf(req, "<p>");
    mod_webserver_printf(req, "IDF Version : %s<br>", esp_get_idf_version());
    mod_webserver_printf(req, "Model : %s<br>", info.model == CHIP_ESP8266 ? "ESP8266" : info.model == CHIP_ESP32 ? "ESP32" : "Unknown");
    mod_webserver_printf(req, "Feature : %s%s%s<br>",
                         info.features & CHIP_FEATURE_WIFI_BGN ? "802.11bgn" : "",
                         info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
                         info.features & CHIP_FEATURE_BT ? "/BT" : "");
    mod_webserver_printf(req, "Cores : %d<br>", info.cores);
    mod_webserver_printf(req, "Revision : %d<br>", info.revision);
    mod_webserver_printf(req, "Flash : %dMB (%s)<br>",
                         spi_flash_get_chip_size() / (1024 * 1024),
                         info.features & CHIP_FEATURE_EMB_FLASH ? "Embedded-Flash" : "External-Flash");
    mod_webserver_printf(req, "Free Heap : %u<br>", esp_get_free_heap_size());
    mod_webserver_printf(req, "</p>");

    // Tail
    mod_webserver_printf(req, "</body>");
    mod_webserver_printf(req, "</html>");
    mod_webserver_printf(req, "", 0);

    return ESP_OK;
}

static esp_err_t restart_get_handler(httpd_req_t *req)
{
    esp_restart();

    return ESP_OK;
}

static httpd_uri_t home = {
    .uri        = "/",
    .method     = HTTP_GET,
    .handler    = home_get_handler,
};

static httpd_uri_t restart = {
    .uri        = "/restart",
    .method     = HTTP_GET,
    .handler    = restart_get_handler,
};

httpd_handle_t mod_webserver_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &home);
        httpd_register_uri_handler(server, &restart);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void mod_webserver_stop(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

void mod_webserver_printf(httpd_req_t *req, const char* format, ...)
{
    char buffer[128];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 128, format, args);
    va_end(args);

    httpd_resp_send_chunk(req, buffer, strlen(buffer));
}
