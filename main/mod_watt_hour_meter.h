/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_WATT_HOUR_METER_H_
#define _MOD_WATT_HOUR_METER_H_

#include <driver/gpio.h>

#include <esp_http_server.h>

extern unsigned short PULSE_PER_HOUR[32][24];
extern unsigned char AREA_NAME[16];
extern int64_t CURRENT_TIME;
extern int64_t PREVIOUS_TIME;

void mod_watt_hour_meter(gpio_num_t gpio_num);
void mod_watt_hour_meter_http_handler(httpd_req_t *req);

#endif
