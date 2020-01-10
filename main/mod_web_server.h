/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_WEB_SERVER_H_
#define _MOD_WEB_SERVER_H_

#include <esp_http_server.h>

httpd_handle_t mod_webserver_start(void);
void mod_webserver_stop(httpd_handle_t server);

void mod_webserver_printf(httpd_req_t *req, const char* format, ...);

#endif
