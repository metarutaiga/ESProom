/*
 * Driver for Bosch Sensortec BME680 digital temperature, humidity, pressure
 * and gas sensor connected to I2C or SPI
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ---------------------------------------------------------------------------
 *
 * The BSD License (3-clause license)
 *
 * Copyright (c) 2017 Gunar Schorcht (https://github.com/gschorcht)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Platform file: platform specific definitions, includes and functions
 */

#include <sys/time.h>

#include "bme680_platform.h"

#ifdef SPI_USED

// platform specific SPI functions

static const spi_settings_t bus_settings = {
    .mode         = SPI_MODE0,
    .freq_divider = SPI_FREQ_DIV_1M,
    .msb          = true,
    .minimal_pins = false,
    .endianness   = SPI_LITTLE_ENDIAN
};

#endif // SPI_USED

bool spi_device_init (uint8_t bus, uint8_t cs)
{
#ifdef SPI_USED
    gpio_enable(cs, GPIO_OUTPUT);
    gpio_write(cs, true);
#endif // SPI_USED
    return true;
}

size_t spi_transfer_pf(uint8_t bus, uint8_t cs, const uint8_t *mosi, uint8_t *miso, uint16_t len)
{
#ifdef SPI_USED
    spi_settings_t old_settings;

    spi_get_settings(bus, &old_settings);
    spi_set_settings(bus, &bus_settings);
    gpio_write(cs, false);

    size_t transfered = spi_transfer (bus, (const void*)mosi, (void*)miso, len, SPI_8BIT);

    gpio_write(cs, true);
    spi_set_settings(bus, &old_settings);
    
    return transfered;
#else
    return 0;
#endif // SPI_USED
}

uint32_t sdk_system_get_time()
{
    struct timeval time;
    gettimeofday(&time, 0);
    return time.tv_sec * 1e6 + time.tv_usec;
}

// esp-open-rtos I2C interface wrapper

#define I2C_ACK_VAL  0x0
#define I2C_NACK_VAL 0x1

void i2c_init(int bus, gpio_num_t scl, gpio_num_t sda, uint32_t freq)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.clk_stretch_tick = CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ * 1000000 / freq;
    i2c_driver_install(bus, conf.mode);
    i2c_param_config(bus, &conf);
}

int i2c_slave_write(uint8_t bus, uint8_t addr, const uint8_t *reg, 
                    uint8_t *data, uint32_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    if (reg)
        i2c_master_write_byte(cmd, *reg, true);
    if (data)
        i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    return err;
}

int i2c_slave_read(uint8_t bus, uint8_t addr, const uint8_t *reg, 
                   uint8_t *data, uint32_t len)
{
    if (len == 0) return true;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (reg)
    {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, *reg, true);
        if (!data)
            i2c_master_stop(cmd);
    }
    if (data)
    {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
        if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_ACK_VAL);
        i2c_master_read_byte(cmd, data + len - 1, I2C_NACK_VAL);
        i2c_master_stop(cmd);
    }
    esp_err_t err = i2c_master_cmd_begin(bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return err;
}
