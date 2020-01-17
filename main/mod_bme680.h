/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_BME680_H_
#define _MOD_BME680_H_

#include <driver/gpio.h>

#include <esp_http_server.h>

extern float BME680_TEMPERATURE;
extern float BME680_HUMIDITY;
extern float BME680_PRESSURE;
extern float BME680_GAS_RESISTANCE;
extern float BME680_AIR_QUALITY;

void mod_bme680(gpio_num_t scl, gpio_num_t sda);

void mod_bme680_http_handler(httpd_req_t *req);

#endif
