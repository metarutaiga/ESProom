/* ESP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _MOD_LOG_H_
#define _MOD_LOG_H_

extern unsigned char LOG_BUFFER[8][128];
extern unsigned char LOG_INDEX;

void mod_log(void);

#endif
