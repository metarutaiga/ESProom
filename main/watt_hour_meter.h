/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _WATT_HOUR_METER_H_
#define _WATT_HOUR_METER_H_

extern unsigned short PULSE_PER_HOUR[32][24];
extern unsigned char AREA_NAME[16];
extern unsigned char LOG_BUFFER[8][128];
extern unsigned char LOG_INDEX;
extern int64_t CURRENT_TIME;
extern int64_t PREVIOUS_TIME;

#endif
