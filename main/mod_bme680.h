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

extern int64_t BME680_TIMESTAMP;
extern float BME680_IAQ;
extern uint8_t BME680_IAQ_ACCURACY;
extern float BME680_STATIC_IAQ;
extern uint8_t BME680_STATIC_IAQ_ACCURACY;
extern float BME680_CO2_EQUIVALENT;
extern uint8_t BME680_CO2_EQUIVALENT_ACCURACY;
extern float BME680_BREATH_VOC_EQUIVALENT;
extern uint8_t BME680_BREATH_VOC_EQUIVALENT_ACCURACY;
extern float BME680_RAW_TEMPERATURE;
extern float BME680_RAW_PRESSURE;
extern float BME680_RAW_HUMIDITY;
extern float BME680_RAW_GAS;
extern float BME680_STABILIZATION_STATUS;
extern float BME680_RUN_IN_STATUS;
extern float BME680_SENSOR_HEAT_COMPENSATED_TEMPERATURE;
extern float BME680_SENSOR_HEAT_COMPENSATED_HUMIDITY;
extern float BME680_COMPENSATED_GAS;
extern uint8_t BME680_COMPENSATED_GAS_ACCURACY;
extern float BME680_GAS_PERCENTAGE;
extern uint8_t BME680_GAS_PERCENTAGE_ACCURACY;

void mod_bme680(gpio_num_t scl, gpio_num_t sda);

void mod_bme680_http_handler(httpd_req_t *req);

#endif
