/* ESProom

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "bme680.h"

#include "mod_web_server.h"
#include "mod_bme680.h"

// I2C interface defintions for ESP32 and ESP8266
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

float BME680_TEMPERATURE;
float BME680_HUMIDITY;
float BME680_PRESSURE;
float BME680_GAS_RESISTANCE;
float BME680_AIR_QUALITY;

static float BME680_GAS_BURN_IN[50];
static unsigned char BME680_GAS_BURN_IN_INDEX;
static float BME680_GAS_BASELINE;

static const char * const TAG = "BME680";

static void mod_bme680_task(void *parameters)
{
    bme680_sensor_t *sensor = (bme680_sensor_t *)parameters;

    TickType_t last_wakeup = xTaskGetTickCount();

    // as long as sensor configuration isn't changed, duration is constant
    uint32_t duration = bme680_get_measurement_duration(sensor);

    while (1)
    {
        // trigger the sensor to start one TPHG measurement cycle
        if (bme680_force_measurement(sensor))
        {
            // passive waiting until measurement results are available
            vTaskDelay(duration);

            // alternatively: busy waiting until measurement results are available
            // while (bme680_is_measuring(sensor)) ;

            // get the results and do something with them
            bme680_values_float_t values;
            if (bme680_get_results_float(sensor, &values))
            {
                BME680_TEMPERATURE = values.temperature;
                BME680_HUMIDITY = values.humidity;
                BME680_PRESSURE = values.pressure;
                BME680_GAS_RESISTANCE = values.gas_resistance;

                // https://github.com/pimoroni/bme680-python/blob/master/examples/indoor-air-quality.py
                if (BME680_GAS_BURN_IN[0] == 0.0f)
                {
                    for (unsigned char i = 0; i < 50; ++i)
                        BME680_GAS_BURN_IN[i] = values.gas_resistance;
                    BME680_GAS_BASELINE = values.gas_resistance;
                }

                unsigned char index = BME680_GAS_BURN_IN_INDEX++;
                if (BME680_GAS_BURN_IN_INDEX >= 50)
                    BME680_GAS_BURN_IN_INDEX = 0;
                BME680_GAS_BASELINE -= BME680_GAS_BURN_IN[index] / 50.0f;
                BME680_GAS_BURN_IN[index] = values.gas_resistance;
                BME680_GAS_BASELINE += BME680_GAS_BURN_IN[index] / 50.0f;

                // Collect gas resistance burn-in values, then use the average
                // of the last 50 values to set the upper limit for calculating
                // gas_baseline.
                float gas_baseline = BME680_GAS_BASELINE;

                // Set the humidity baseline to 40%, an optimal indoor humidity.
                float hum_baseline = 40.0f;

                // This sets the balance between humidity and gas reading in the
                // calculation of air_quality_score (25:75, humidity:gas)
                float hum_weighting = 0.25f;

                float gas = values.gas_resistance;
                float gas_offset = gas_baseline - gas;

                float hum = values.humidity;
                float hum_offset = hum - hum_baseline;

                // Calculate hum_score as the distance from the hum_baseline.
                float hum_score;
                if (hum_offset > 0.0f)
                {
                    hum_score = (100.0f - hum_baseline - hum_offset);
                    hum_score /= (100.0f - hum_baseline);
                    hum_score *= (hum_weighting * 100.0f);
                }
                else
                {
                    hum_score = (hum_baseline + hum_offset);
                    hum_score /= hum_baseline;
                    hum_score *= (hum_weighting * 100.0f);
                }

                // Calculate gas_score as the distance from the gas_baseline.
                float gas_score;
                if (gas_offset > 0.0f)
                {
                    gas_score = (gas / gas_baseline);
                    gas_score *= (100.0f - (hum_weighting * 100.0f));
                }
                else
                {
                    gas_score = 100.0f - (hum_weighting * 100.0f);
                }

                // Calculate air_quality_score.
                float air_quality_score = hum_score + gas_score;

                BME680_AIR_QUALITY = air_quality_score;
#if 0
                ESP_LOGI(TAG, "%.2f °C, %.2f %%, %.2f hPa, %.2f Ohm",
                         values.temperature, values.humidity,
                         values.pressure, values.gas_resistance);
#endif
            }
        }

        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}

void mod_bme680(gpio_num_t scl, gpio_num_t sda)
{
    i2c_init(I2C_BUS, scl, sda, I2C_FREQ);
    bme680_sensor_t* sensor = bme680_init_sensor(I2C_BUS, BME680_I2C_ADDRESS_1, 0);

    if (sensor)
    {
        /** -- SENSOR CONFIGURATION PART (optional) --- */

        // Changes the oversampling rates to 8x oversampling for temperature
        // and 2x oversampling for humidity. Pressure measurement is 4x.
        bme680_set_oversampling_rates(sensor, osr_8x, osr_4x, osr_2x);

        // Change the IIR filter size for temperature and pressure to 3.
        bme680_set_filter_size(sensor, iir_size_3);

        // Change the heater profile 0 to 320 degree Celcius for 150 ms.
        bme680_set_heater_profile(sensor, 0, 320, 150);
        bme680_use_heater_profile(sensor, 0);

        // Set ambient temperature to 25 degree Celsius
        bme680_set_ambient_temperature(sensor, 25);
            
        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor 
        // configuration part

        // Create a task that uses the sensor
        xTaskCreate(mod_bme680_task, "mod_bme680_task", 2048, sensor, 2, NULL);
    }
    else
        printf("Could not initialize BME680 sensor\n");
}

void mod_bme680_http_handler(httpd_req_t *req)
{
    if (BME680_GAS_RESISTANCE == 0.0f)
        return;

    mod_webserver_printf(req, "<p>");
    mod_webserver_printf(req, "Temperature : %.2f °C<br>", BME680_TEMPERATURE);
    mod_webserver_printf(req, "Humidity : %.2f %%<br>", BME680_HUMIDITY);
    mod_webserver_printf(req, "Pressure : %.2f hPa<br>", BME680_PRESSURE);
    mod_webserver_printf(req, "Gas Resistance : %.2f Ohm<br>", BME680_GAS_RESISTANCE);
    mod_webserver_printf(req, "Air Quality : %.2f<br>", BME680_AIR_QUALITY);
    mod_webserver_printf(req, "</p>");
}
