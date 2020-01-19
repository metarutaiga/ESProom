/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <driver/i2c.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "bsec_integration.h"

#include "mod_web_server.h"
#include "mod_bme680.h"

#define I2C_ACK_VAL  0x0
#define I2C_NACK_VAL 0x1

int64_t BME680_TIMESTAMP;
float BME680_IAQ;
uint8_t BME680_IAQ_ACCURACY;
float BME680_TEMPERATURE;
float BME680_HUMIDITY;
float BME680_PRESSURE;
float BME680_RAW_TEMPERATURE;
float BME680_RAW_HUMIDITY;
float BME680_GAS_RESISTANCE;
float BME680_STATIC_IAQ;
float BME680_CO2_EQUIVALENT;
float BME680_BREATH_VOC_EQUIVALENT;

static const char * const TAG = "BME680";

static void bus_init(int bus, gpio_num_t scl, gpio_num_t sda)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.clk_stretch_tick = CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ * 1000000 / 100000;
    i2c_driver_install(bus, conf.mode);
    i2c_param_config(bus, &conf);
}

static int8_t bus_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data_ptr, uint16_t data_len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data_ptr, data_len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    return err;
}

static int8_t bus_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data_ptr, uint16_t data_len)
{
    if (data_len == 0)
        return 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    if (reg_data_ptr)
    {
        i2c_master_start(cmd); 
        i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
        if (data_len > 1)
            i2c_master_read(cmd, reg_data_ptr, data_len - 1, I2C_ACK_VAL);
        i2c_master_read_byte(cmd, reg_data_ptr + data_len - 1, I2C_NACK_VAL);
    }
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return err;
}

static void sleep(uint32_t t_ms)
{
    vTaskDelay(t_ms / portTICK_PERIOD_MS);
}

static int64_t get_timestamp_us()
{
    return esp_timer_get_time();
}

static void output_ready(int64_t timestamp, float iaq, uint8_t iaq_accuracy, float temperature, float humidity,
    float pressure, float raw_temperature, float raw_humidity, float gas, bsec_library_return_t bsec_status,
    float static_iaq, float co2_equivalent, float breath_voc_equivalent)
{
    BME680_TIMESTAMP = timestamp;
    BME680_IAQ = iaq;
    BME680_IAQ_ACCURACY = iaq_accuracy;
    BME680_TEMPERATURE = temperature;
    BME680_HUMIDITY = humidity;
    BME680_PRESSURE = pressure / 100.0f;
    BME680_RAW_TEMPERATURE = raw_temperature;
    BME680_RAW_HUMIDITY = raw_humidity;
    BME680_GAS_RESISTANCE = gas;
    BME680_STATIC_IAQ = static_iaq;
    BME680_CO2_EQUIVALENT = co2_equivalent;
    BME680_BREATH_VOC_EQUIVALENT = breath_voc_equivalent;
}

static uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer)
{
    return 0;
}

static void state_save(const uint8_t *state_buffer, uint32_t length)
{
}
 
static uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer)
{
    return 0;
}

static void mod_bme680_task(void *parameters)
{
    /* Call to endless loop function which reads and processes data based on sensor settings */
    /* State is saved every 10.000 samples, which means every 10.000 * 3 secs = 500 minutes  */
    bsec_iot_loop(sleep, get_timestamp_us, output_ready, state_save, 10000);
}

void mod_bme680(gpio_num_t scl, gpio_num_t sda)
{
    bus_init(0, scl, sda);

    /* Call to the function which initializes the BSEC library 
     * Switch on low-power mode and provide no temperature offset */
    return_values_init ret = bsec_iot_init(BSEC_SAMPLE_RATE_LP, 0.0f, bus_write, bus_read, sleep, state_load, config_load);
    if (ret.bme680_status)
    {
        /* Could not intialize BME680 */
        ESP_LOGE(TAG, "Could not intialize BME680");
        return;
    }
    else if (ret.bsec_status)
    {
        /* Could not intialize BSEC library */
        ESP_LOGE(TAG, "Could not intialize BSEC library");
        return;
    }
    
    // Create a task that uses the sensor
    xTaskCreate(mod_bme680_task, "mod_bme680_task", 2048, NULL, 2, NULL);
}

void mod_bme680_http_handler(httpd_req_t *req)
{
    if (BME680_TIMESTAMP == 0)
        return;

    bsec_version_t version;
    bsec_get_version(&version);

    mod_webserver_printf(req, "<p>");
    mod_webserver_printf(req, "BSEC version: %d.%d.%d.%d<br>", version.major, version.minor, version.major_bugfix, version.minor_bugfix);
    mod_webserver_printf(req, "Timestamp : %u ms<br>", BME680_TIMESTAMP / 1000000);
    mod_webserver_printf(req, "Indoor Air Quality : %.2f<br>", BME680_IAQ);
    mod_webserver_printf(req, "Indoor Air Quality Accuracy : %u<br>", BME680_IAQ_ACCURACY);
    mod_webserver_printf(req, "Temperature : %.2f °C<br>", BME680_TEMPERATURE);
    mod_webserver_printf(req, "Humidity : %.2f %%<br>", BME680_HUMIDITY);
    mod_webserver_printf(req, "Pressure : %.2f hPa<br>", BME680_PRESSURE);
    mod_webserver_printf(req, "Raw Temperature : %.2f °C<br>", BME680_RAW_TEMPERATURE);
    mod_webserver_printf(req, "Raw Humidity : %.2f %%<br>", BME680_RAW_HUMIDITY);
    mod_webserver_printf(req, "Gas Resistance : %.2f Ohm<br>", BME680_GAS_RESISTANCE);
    mod_webserver_printf(req, "Static Indoor Air Quality : %.2f<br>", BME680_STATIC_IAQ);
    mod_webserver_printf(req, "CO2 Equivalent : %.2f<br>", BME680_CO2_EQUIVALENT);
    mod_webserver_printf(req, "Breath VOC Equivalent : %.2f<br>", BME680_BREATH_VOC_EQUIVALENT);
    mod_webserver_printf(req, "</p>");
}
