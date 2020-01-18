/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>

#include "mod_web_server.h"
#include "mod_log.h"

unsigned char LOG_BUFFER[8][128];
unsigned char LOG_INDEX;

static putchar_like_t orig_putchar = NULL;

static int mod_putchar(int ch)
{
    static int line = 0;
    static int color = 0;

    if (ch == '\n') {
        LOG_INDEX = (LOG_INDEX + 1) % 8;
        line = 0;
    }
    else if (ch == '\033') {
        color = 1;
    }
    else if (ch == 'm' && color == 1) {
        color = 0;
    }
    else if (line < (sizeof(LOG_BUFFER[LOG_INDEX]) - 1) && color == 0) {
        LOG_BUFFER[LOG_INDEX][line++] = ch;
        LOG_BUFFER[LOG_INDEX][line] = 0;
    }

    if (orig_putchar)
        return orig_putchar(ch);

    return ESP_OK;
}

void mod_log(void)
{
    orig_putchar = esp_log_set_putchar(mod_putchar);
}

void mod_log_http_handler(httpd_req_t *req)
{
    mod_webserver_printf(req, "<p>");
    for (int i = 0; i < 8; ++i) {
        mod_webserver_printf(req, "Log : %s", LOG_BUFFER[(LOG_INDEX + i) % 8]);
        mod_webserver_printf(req, "<br>");
    }
    mod_webserver_printf(req, "</p>");
}
