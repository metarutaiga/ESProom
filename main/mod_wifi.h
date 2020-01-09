/* ESP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_WIFI_H_
#define _MOD_WIFI_H_

void mod_wifi(void);
void mod_wifi_update(void);
void mod_wifi_wait_connected(void);
void mod_wifi_restart(void);
int mod_wifi_failed_count(void);
int mod_wifi_restart_count(void);

#endif
