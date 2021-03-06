/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_WIFI_H_
#define _MOD_WIFI_H_

#include <esp_http_server.h>

void mod_wifi(void);
void mod_wifi_update(void);
void mod_wifi_wait_connected(void);
void mod_wifi_restart(void);

void mod_wifi_http_handler(httpd_req_t *req);

#endif
