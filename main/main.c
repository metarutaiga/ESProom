/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>

#include <driver/gpio.h>
#include <esp8266/pin_mux_register.h>

#include <nvs_flash.h>

#include "mod_bme680.h"
#include "mod_log.h"
#include "mod_mqtt.h"
#include "mod_ota.h"
#include "mod_sntp.h"
#include "mod_watt_hour_meter.h"
#include "mod_web_server.h"
#include "mod_wifi.h"

void app_main()
{
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

    // Initialize
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    mod_log();
    mod_wifi();
    mod_sntp();
    mod_watt_hour_meter(GPIO_NUM_2);
    mod_mqtt();
    mod_bme680(GPIO_NUM_3, GPIO_NUM_0);

    httpd_handle_t server = mod_webserver_start();
    mod_ota(server);

    for (;;) {
        mod_wifi_update();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
