/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <freertos/event_groups.h>

#include "app_wifi.h"

static const char * const TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static int wifi_start = 0;
static int wifi_connected = 0;
static int wifi_restart = 0;
static int wifi_failed_count = 0;
static int wifi_reconnected_count = 0;
static int wifi_restart_count = 0;

static unsigned char wifi_ssids[8][16];
static unsigned char wifi_passwords[8][16];
static unsigned char wifi_best_ssid;

static void app_wifi_scan_all()
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list;
    char best_rssi = -128;
    unsigned char best_ssid = 0;

    esp_wifi_scan_get_ap_num(&ap_count);	
    if (ap_count <= 0) {
        esp_restart();
        return;
    }

    ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));	

    ESP_LOGI(TAG, "======================================================================");
    ESP_LOGI(TAG, "             SSID             |    RSSI    |           AUTH           ");
    ESP_LOGI(TAG, "======================================================================");
    for (uint16_t i = 0; i < ap_count; i++) {
	char *authmode;
        switch (ap_list[i].authmode) {
            case WIFI_AUTH_OPEN:
                authmode = "WIFI_AUTH_OPEN";
	        break;
	    case WIFI_AUTH_WEP:
                authmode = "WIFI_AUTH_WEP";
                break;        
            case WIFI_AUTH_WPA_PSK:
                authmode = "WIFI_AUTH_WPA_PSK";
                break;           
            case WIFI_AUTH_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA2_PSK";
                break;           
            case WIFI_AUTH_WPA_WPA2_PSK:
                authmode = "WIFI_AUTH_WPA_WPA2_PSK";
                break;
            default:
                authmode = "Unknown";
                break;
    	}
        ESP_LOGI(TAG, "%26.26s    |    % 4d    |    %22.22s", ap_list[i].ssid, ap_list[i].rssi, authmode);

        for (unsigned char j = 0; j < 8; j++) {
            if (wifi_ssids[j][0] == 0)
                continue;
            if (strcmp((char*)ap_list[i].ssid, (char*)wifi_ssids[j]) == 0) {
                if (best_rssi > ap_list[i].rssi) {
                    best_rssi = ap_list[i].rssi;
                    best_ssid = j;
                }
            }
        }
    }
    free(ap_list);

    wifi_best_ssid = best_ssid;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
#if 0
    uint8_t mac[6];
    char hostname[16];
#endif
    wifi_config_t wifi_config;

    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");

            esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
#if 0
            esp_wifi_get_mac(WIFI_IF_STA, mac);
            sprintf(hostname, "ESP_%02X%02X%02X", mac[3], mac[4], mac[5]);    
            tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
            ESP_LOGI(TAG, "Hostname : %s", hostname);
#endif

            wifi_start = 1;
            break;
        case SYSTEM_EVENT_STA_STOP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");

            wifi_start = 0;
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");

            wifi_connected = 1;
            wifi_reconnected_count = 0;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
            if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
                /*Switch to 802.11 bgn mode */
                esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
            }

            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

            wifi_connected = 0;
            wifi_failed_count++;
            wifi_reconnected_count++;
            if ((wifi_reconnected_count % 10) == 0) {
                app_wifi_restart();
            }
            if ((wifi_reconnected_count / 10) >= 10) {
                esp_restart();
            }
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "Got IP: '%s'", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");

            app_wifi_scan_all();

            memset(&wifi_config, 0, sizeof(wifi_config));
            strcpy((char*)wifi_config.sta.ssid, (char*)wifi_ssids[wifi_best_ssid]);
            strcpy((char*)wifi_config.sta.password, (char*)wifi_passwords[wifi_best_ssid]);
            ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
            esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

            esp_wifi_connect();
            break;
        default:
            ESP_LOGI(TAG, "SYSTEM_EVENT_UNKNOWN (%d)", event->event_id);
            break;
    }
    return ESP_OK;
}

void app_wifi_prepare()
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
}

void app_wifi_initialise()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_country_t country = {
        .cc = "JP",
        .schan = 1,
        .nchan = 14,
        .max_tx_power = 127,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = 1,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };

    const char* ssid = CONFIG_WIFI_SSID;
    const char* password = CONFIG_WIFI_PASSWORD;

    memset(wifi_ssids, 0, sizeof(wifi_ssids));
    for (unsigned char i = 0, j = 0, c = 1; c != 0; ssid++) {
        c = (*ssid);
        if (c == ';') {
            i++;
            j = 0;
            continue;
        }
        wifi_ssids[i][j++] = c;
    }

    memset(wifi_passwords, 0, sizeof(wifi_passwords));
    for (unsigned char i = 0, j = 0, c = 1; c != 0; password++) {
        c = (*password);
        if (c == ';') {
            i++;
            j = 0;
            continue;
        }
        wifi_passwords[i][j++] = c;
    }

    wifi_best_ssid = 0;
    strcpy((char*)wifi_config.sta.ssid, (char*)wifi_ssids[wifi_best_ssid]);
    strcpy((char*)wifi_config.sta.password, (char*)wifi_passwords[wifi_best_ssid]);

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, WIFI_BW_HT40));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
}

void app_wifi_shutdown()
{
    ESP_LOGI(TAG, "Shutdown WiFi...");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    while (wifi_connected == 1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    while (wifi_start == 1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

void app_wifi_wait_connected()
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 10000 / portTICK_PERIOD_MS);
}

void app_wifi_loop()
{
    for (;;) {
        if (wifi_restart == 1) {
            wifi_restart = 0;
            app_wifi_shutdown();
            app_wifi_initialise();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_wifi_restart()
{
    wifi_restart = 1;
    wifi_restart_count++;
}

int app_wifi_failed_count()
{
    return wifi_failed_count;
}

int app_wifi_restart_count()
{
    return wifi_restart_count;
}
