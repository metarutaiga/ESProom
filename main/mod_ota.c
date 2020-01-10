/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_server.h>

#include "mod_log.h"
#include "mod_ota.h"

#define BUFFSIZE 1500
#define TEXT_BUFFSIZE 1024

typedef enum esp_ota_firm_state {
    ESP_OTA_INIT = 0,
    ESP_OTA_PREPARE,
    ESP_OTA_START,
    ESP_OTA_RECVED,
    ESP_OTA_FINISH,
} esp_ota_firm_state_t;

typedef struct esp_ota_firm {
    uint8_t             ota_num;
    uint8_t             update_ota_num;

    esp_ota_firm_state_t    state;

    size_t              content_len;

    size_t              read_bytes;
    size_t              write_bytes;

    size_t              ota_size;
    size_t              ota_offset;

    const char          *buf;
    size_t              bytes;
} esp_ota_firm_t;

/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;
/*fatal error*/
static int fatal_error = 0;
/*http log*/
static httpd_req_t *log_req = NULL;

static const char *TAG = "OTA";

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(const char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

static void ota_http_log()
{
    if (log_req == NULL)
        return;

    int log_index = (LOG_INDEX - 1) % 8;
    char* log_buffer = (char*)LOG_BUFFER[log_index];
    int len = strlen(log_buffer);
    if (len) {
        httpd_resp_send_chunk(log_req, log_buffer, len);
        httpd_resp_send_chunk(log_req, "<br>", strlen("<br>"));
    }
}

static bool connect_to_http_server()
{
    ESP_LOGI(TAG, "Server IP: %s Server Port:%s", CONFIG_OTA_SERVER_IP, CONFIG_OTA_SERVER_PORT);
    ota_http_log();

    int  http_connect_flag = -1;
    struct sockaddr_in sock_info;

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        ota_http_log();
        return false;
    }

    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(CONFIG_OTA_SERVER_IP);
    sock_info.sin_port = htons(atoi(CONFIG_OTA_SERVER_PORT));

    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        ota_http_log();
        close(socket_id);
        return false;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        ota_http_log();
        return true;
    }
    return false;
}

static void task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    ota_http_log();
    close(socket_id);
    fatal_error = 1;
    vTaskDelete(NULL);
}

bool _esp_ota_firm_parse_http(esp_ota_firm_t *ota_firm, const char *text, size_t total_len, size_t *parse_len)
{
    /* i means current position */
    int i = 0, i_read_len = 0;
    char *ptr = NULL, *ptr2 = NULL;
    char length_str[32];

    while (text[i] != 0 && i < total_len) {
        if (ota_firm->content_len == 0 && (ptr = (char *)strstr(text, "Content-Length")) != NULL) {
            ptr += 16;
            ptr2 = (char *)strstr(ptr, "\r\n");
            memset(length_str, 0, sizeof(length_str));
            memcpy(length_str, ptr, ptr2 - ptr);
            ota_firm->content_len = atoi(length_str);
            ota_firm->ota_size = ota_firm->content_len / ota_firm->ota_num;
            ota_firm->ota_offset = ota_firm->ota_size * ota_firm->update_ota_num;
            ESP_LOGI(TAG, "parse Content-Length:%d, ota_size %d", ota_firm->content_len, ota_firm->ota_size);
            ota_http_log();
        }

        i_read_len = read_until(&text[i], '\n', total_len - i);

        if (i_read_len > total_len - i) {
            ESP_LOGE(TAG, "recv malformed http header");
            ota_http_log();
            task_fatal_error();
        }

        // if resolve \r\n line, http header is finished
        if (i_read_len == 2) {
            if (ota_firm->content_len == 0) {
                ESP_LOGE(TAG, "did not parse Content-Length item");
                ota_http_log();
                task_fatal_error();
            }

            *parse_len = i + 2;

            return true;
        }

        i += i_read_len;
    }

    return false;
}

static size_t esp_ota_firm_do_parse_msg(esp_ota_firm_t *ota_firm, const char *in_buf, size_t in_len)
{
    size_t tmp;
    size_t parsed_bytes = in_len; 

    switch (ota_firm->state) {
        case ESP_OTA_INIT:
            if (_esp_ota_firm_parse_http(ota_firm, in_buf, in_len, &tmp)) {
                ota_firm->state = ESP_OTA_PREPARE;
                ESP_LOGD(TAG, "Http parse %d bytes", tmp);
                parsed_bytes = tmp;
            }
            break;
        case ESP_OTA_PREPARE:
            ota_firm->read_bytes += in_len;

            if (ota_firm->read_bytes >= ota_firm->ota_offset) {
                ota_firm->buf = &in_buf[in_len - (ota_firm->read_bytes - ota_firm->ota_offset)];
                ota_firm->bytes = ota_firm->read_bytes - ota_firm->ota_offset;
                ota_firm->write_bytes += ota_firm->read_bytes - ota_firm->ota_offset;
                ota_firm->state = ESP_OTA_START;
                ESP_LOGD(TAG, "Receive %d bytes and start to update", ota_firm->read_bytes);
                ESP_LOGD(TAG, "Write %d total %d", ota_firm->bytes, ota_firm->write_bytes);
            }

            break;
        case ESP_OTA_START:
            if (ota_firm->write_bytes + in_len > ota_firm->ota_size) {
                ota_firm->bytes = ota_firm->ota_size - ota_firm->write_bytes;
                ota_firm->state = ESP_OTA_RECVED;
            } else
                ota_firm->bytes = in_len;

            ota_firm->buf = in_buf;

            ota_firm->write_bytes += ota_firm->bytes;

            ESP_LOGD(TAG, "Write %d total %d", ota_firm->bytes, ota_firm->write_bytes);
            break;
        case ESP_OTA_RECVED:
            parsed_bytes = 0;
            ota_firm->state = ESP_OTA_FINISH;
            break;
        default:
            parsed_bytes = 0;
            ESP_LOGD(TAG, "State is %d", ota_firm->state);
            break;
    }

    return parsed_bytes;
}

static void esp_ota_firm_parse_msg(esp_ota_firm_t *ota_firm, const char *in_buf, size_t in_len)
{
    size_t parse_bytes = 0;

    ESP_LOGD(TAG, "Input %d bytes", in_len);

    do {
        size_t bytes = esp_ota_firm_do_parse_msg(ota_firm, in_buf + parse_bytes, in_len - parse_bytes);
        ESP_LOGD(TAG, "Parse %d bytes", bytes);
        if (bytes)
            parse_bytes += bytes;
    } while (parse_bytes != in_len);
}

static inline int esp_ota_firm_is_finished(esp_ota_firm_t *ota_firm)
{
    return (ota_firm->state == ESP_OTA_FINISH || ota_firm->state == ESP_OTA_RECVED);
}

static inline int esp_ota_firm_can_write(esp_ota_firm_t *ota_firm)
{
    return (ota_firm->state == ESP_OTA_START || ota_firm->state == ESP_OTA_RECVED);
}

static inline const char* esp_ota_firm_get_write_buf(esp_ota_firm_t *ota_firm)
{
    return ota_firm->buf;
}

static inline size_t esp_ota_firm_get_write_bytes(esp_ota_firm_t *ota_firm)
{
    return ota_firm->bytes;
}

static void esp_ota_firm_init(esp_ota_firm_t *ota_firm, const esp_partition_t *update_partition)
{
    memset(ota_firm, 0, sizeof(esp_ota_firm_t));
    ota_firm->state = ESP_OTA_INIT;
    ota_firm->ota_num = get_ota_partition_count();
    ota_firm->update_ota_num = update_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;

    ESP_LOGI(TAG, "Totoal OTA number %d update to %d part", ota_firm->ota_num, ota_firm->update_ota_num);
    ota_http_log();
}

static void ota_task(void *parameter)
{
    httpd_req_t *req = (httpd_req_t *)parameter;

    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    const char* html_header = "<!DOCTYPE html>"
                              "<html>"
                              "<body>";
    httpd_resp_send_chunk(req, html_header, strlen(html_header));

    ESP_LOGI(TAG, "Starting OTA ... @ %p flash %s", ota_task, CONFIG_ESPTOOLPY_FLASHSIZE);
    ota_http_log();

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ota_http_log();
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
        ota_http_log();
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
    ota_http_log();

    ESP_LOGI(TAG, "Start to Connect to Server....");
    ota_http_log();

    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG, "Connected to http server");
        ota_http_log();
    } else {
        ESP_LOGE(TAG, "Connect to http server failed!");
        ota_http_log();
        task_fatal_error();
    }

    /*send GET request to http server*/
    const char *GET_FORMAT =
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%s\r\n"
        "User-Agent: esp-idf/1.0 esp32\r\n\r\n";

    char *http_request = NULL;
    int get_len = asprintf(&http_request, GET_FORMAT, CONFIG_OTA_FILENAME, CONFIG_OTA_SERVER_IP, CONFIG_OTA_SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        ota_http_log();
        task_fatal_error();
    }
    int res = send(socket_id, http_request, get_len, 0);
    free(http_request);

    if (res < 0) {
        ESP_LOGE(TAG, "Send GET request to server failed");
        ota_http_log();
        task_fatal_error();
    } else {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
        ota_http_log();
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    ota_http_log();
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        ota_http_log();
        task_fatal_error();
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    ota_http_log();

    bool flag = true;
    esp_ota_firm_t ota_firm;

    esp_ota_firm_init(&ota_firm, update_partition);

    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(ota_write_data, 0, BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            ota_http_log();
            task_fatal_error();
        } else if (buff_len > 0) { /*deal with response body*/
            esp_ota_firm_parse_msg(&ota_firm, text, buff_len);

            if (!esp_ota_firm_can_write(&ota_firm))
                continue;

            memcpy(ota_write_data, esp_ota_firm_get_write_buf(&ota_firm), esp_ota_firm_get_write_bytes(&ota_firm));
            buff_len = esp_ota_firm_get_write_bytes(&ota_firm);

            err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                ota_http_log();
                task_fatal_error();
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
            ota_http_log();
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            ota_http_log();
            close(socket_id);
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
            ota_http_log();
        }

        if (esp_ota_firm_is_finished(&ota_firm))
            break;
    }

    ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);
    ota_http_log();

    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        ota_http_log();
        esp_restart();
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        ota_http_log();
        esp_restart();
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    ota_http_log();
    esp_restart();
    return;
}

static esp_err_t ota_get_handler(httpd_req_t *req)
{
    static int enter = 0;

    enter++;
    fatal_error = 0;
    log_req = req;
    if (enter == 1) {
        xTaskCreate(&ota_task, "ota_task", 8192, req, 5, NULL);
    }
    for (int i = 0; i < 10 * 60; ++i) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (fatal_error != 0)
            break;
    }
    fatal_error = 0;
    log_req = NULL;
    enter--;

    return ESP_OK;
}

httpd_uri_t ota = {
    .uri       = "/ota",
    .method    = HTTP_GET,
    .handler   = ota_get_handler,
};

void mod_ota(httpd_handle_t server)
{
    httpd_register_uri_handler(server, &ota);
}
