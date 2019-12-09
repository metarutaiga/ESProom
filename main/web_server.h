/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _WEB_SERVER_H_
#define _WEB_SERVER_H_

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif
