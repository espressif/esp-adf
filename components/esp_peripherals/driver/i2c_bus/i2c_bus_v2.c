/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <sys/queue.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/i2c.h"
#include "audio_mutex.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "i2c_bus.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))

static const char *TAG = "i2c_bus_v2";

#define I2C_BUS_CHECK(a, str, ret)  if(!(a)) {                               \
    ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
    return (ret);                                                            \
}

#define I2C_BUS_PORT_CHECK_RET(port, err) do {      \
    if (port < 0 || port > I2C_NUM_MAX) {           \
        ESP_LOGE(TAG, "Invalid i2c port %d", port); \
        return err;                                 \
    }                                               \
} while (0)

#define DEFAULT_I2C_TRANS_TIMEOUT  (200)   // default i2c transmit timeout (ms)

typedef struct i2c_dev_info_s {
    SLIST_ENTRY(i2c_dev_info_s)  next;
    uint16_t                     dev_addr;
    i2c_master_dev_handle_t      dev_handle;
} i2c_dev_info_t;

typedef struct {
    uint8_t                      port;
    uint32_t                     clk;
} i2c_bus_info_t;

typedef struct {
    i2c_master_bus_handle_t      master_handle;
    int                          ref_count;
    xSemaphoreHandle             bus_lock;
    SLIST_HEAD( ,i2c_dev_info_s) dev_lists;
} i2c_master_info_t;
 
static i2c_master_info_t master[I2C_NUM_MAX];

static i2c_master_dev_handle_t get_i2c_device_handle(i2c_bus_info_t* bus, uint16_t addr)
{
    i2c_dev_info_t* dev_info = NULL;
    SLIST_FOREACH(dev_info, &master[bus->port].dev_lists, next) {
        if (dev_info->dev_addr == addr) {
            return dev_info->dev_handle;
        }
    }
    return NULL;
}

static i2c_master_dev_handle_t add_i2c_device(i2c_bus_info_t* bus, uint16_t addr)
{
    i2c_dev_info_t* dev_info = audio_calloc(1, sizeof(i2c_dev_info_t)); // calloc and put to dev lists
    I2C_BUS_CHECK(dev_info, "Insufficient memory", NULL);
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr >> 1,
        .scl_speed_hz = bus->clk,
    };
    esp_err_t ret = i2c_master_bus_add_device(master[bus->port].master_handle, &cfg, &dev_info->dev_handle);
    AUDIO_RET_ON_FALSE(TAG, ret, return NULL, "Failed to add device to bus");
    dev_info->dev_addr = addr;
    SLIST_INSERT_HEAD(&master[bus->port].dev_lists, dev_info, next);
    return dev_info->dev_handle;
}

static i2c_master_dev_handle_t try_get_i2c_device_handle(i2c_bus_info_t* bus, uint16_t addr)
{
    i2c_master_dev_handle_t handle = get_i2c_device_handle(bus, addr);
    if (handle) {
        return handle;
    }
    return add_i2c_device(bus, addr);
}

static i2c_master_bus_handle_t i2c_master_create(i2c_port_t port, i2c_config_t *cfg)
{
    esp_err_t ret = ESP_OK;
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = port;
    i2c_bus_config.scl_io_num = cfg->scl_io_num;
    i2c_bus_config.sda_io_num = cfg->sda_io_num;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
    if (ret == ESP_OK) {
        return i2c_bus_handle;
    }
    return NULL;
}

i2c_bus_handle_t i2c_bus_create(i2c_port_t port, i2c_config_t *conf)
{
    I2C_BUS_PORT_CHECK_RET(port, NULL);
    I2C_BUS_CHECK(conf != NULL, "Configuration not initialized", NULL);

    if (master[port].master_handle == NULL) {
        ESP_LOGW(TAG, "I2C master handle is NULL, will create new one");
        master[port].master_handle = i2c_master_create(port, conf);
        AUDIO_MEM_CHECK(TAG, master[port].master_handle, return NULL);
    }
    if (master[port].bus_lock == NULL) {
        master[port].bus_lock = mutex_create();
        AUDIO_MEM_CHECK(TAG, master[port].bus_lock, return NULL;);
        SLIST_INIT(&master[port].dev_lists);
    }
    i2c_bus_info_t* bus = audio_calloc(1, sizeof(i2c_bus_info_t));
    bus->port = port;
    bus->clk = conf->master.clk_speed;
    __sync_fetch_and_add(&master[port].ref_count, 1);

    return (i2c_bus_handle_t)bus;
}

esp_err_t i2c_bus_write_bytes(i2c_bus_handle_t bus, int addr, uint8_t *reg, int regLen, uint8_t *data, int datalen)
{
    i2c_bus_info_t* bus_info = (i2c_bus_info_t*)bus;
    i2c_master_dev_handle_t dev_handle = try_get_i2c_device_handle(bus, addr);
    I2C_BUS_CHECK(dev_handle != NULL, "I2C device handle is NULL", ESP_FAIL);

    int write_len = regLen + datalen;
    uint8_t *write_buffer = (uint8_t*)audio_calloc(1, write_len);
    AUDIO_MEM_CHECK(TAG, write_buffer, return ESP_FAIL);

    memcpy(write_buffer, reg, regLen);
    memcpy(write_buffer + regLen, data, datalen);
    mutex_lock(master[bus_info->port].bus_lock);
    int ret = i2c_master_transmit(dev_handle, write_buffer, write_len, DEFAULT_I2C_TRANS_TIMEOUT);
    mutex_unlock(master[bus_info->port].bus_lock);
    AUDIO_RET_ON_FALSE(TAG, ret, {audio_free(write_buffer); return ESP_FAIL;}, "I2C bus write bytes failed");
    audio_free(write_buffer);
    return ESP_OK;
}

esp_err_t i2c_bus_set_master_handle(i2c_port_t port, i2c_master_bus_handle_t master_handle)
{
    I2C_BUS_PORT_CHECK_RET(port, ESP_ERR_INVALID_ARG);
    master[port].master_handle = master_handle;
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_get_master_handle(i2c_port_t port)
{
    I2C_BUS_PORT_CHECK_RET(port, NULL);
    i2c_master_bus_handle_t bus =  master[port].master_handle;
    return bus;
}

esp_err_t i2c_bus_write_data(i2c_bus_handle_t bus, int addr, uint8_t *data, int datalen)
{
    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    i2c_master_dev_handle_t dev_handle = try_get_i2c_device_handle(bus, addr);
    I2C_BUS_CHECK(dev_handle != NULL, "I2C device handle is NULL", ESP_FAIL);

    mutex_lock(master[bus_info->port].bus_lock);
    esp_err_t ret = i2c_master_transmit(dev_handle, data, datalen, DEFAULT_I2C_TRANS_TIMEOUT);
    mutex_unlock(master[bus_info->port].bus_lock);
    AUDIO_RET_ON_FALSE(TAG, ret, {return ESP_FAIL;}, "I2C bus write bytes failed");
    return ESP_OK;
}

esp_err_t i2c_bus_read_bytes(i2c_bus_handle_t bus, int addr, uint8_t *reg, int reglen, uint8_t *outdata, int datalen)
{
    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    i2c_master_dev_handle_t dev_handle = try_get_i2c_device_handle(bus, addr);
    I2C_BUS_CHECK(dev_handle != NULL, "I2C device handle is NULL", ESP_FAIL);

    mutex_lock(master[bus_info->port].bus_lock);
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, reg, reglen, outdata, datalen, DEFAULT_I2C_TRANS_TIMEOUT);
    mutex_unlock(master[bus_info->port].bus_lock);
    AUDIO_RET_ON_FALSE(TAG, ret, {return ESP_FAIL;}, "I2C bus read bytes failed");
    return ESP_OK;
}

esp_err_t i2c_bus_read_bytes_directly(i2c_bus_handle_t bus, int addr, uint8_t *outdata, int datalen)
{
    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    i2c_master_dev_handle_t dev_handle = try_get_i2c_device_handle(bus, addr);
    I2C_BUS_CHECK(dev_handle != NULL, "I2C device handle is NULL", ESP_FAIL);

    mutex_lock(master[bus_info->port].bus_lock);
    esp_err_t ret = i2c_master_receive(dev_handle, outdata, datalen, DEFAULT_I2C_TRANS_TIMEOUT);
    mutex_unlock(master[bus_info->port].bus_lock);
    AUDIO_RET_ON_FALSE(TAG, ret, {return ESP_FAIL;}, "I2C bus read bytes failed");
    return ESP_OK;
}

esp_err_t i2c_bus_delete(i2c_bus_handle_t bus)
{
    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    __sync_fetch_and_sub(&master[bus_info->port].ref_count, 1);
    if (master[bus_info->port].ref_count == 0) {
        mutex_destroy(master[bus_info->port].bus_lock);
        master[bus_info->port].bus_lock = NULL;
    }
    audio_free(bus);
    return ESP_OK;
}

esp_err_t i2c_bus_probe_addr(i2c_bus_handle_t bus, uint8_t addr)
{
    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    esp_err_t ret = ESP_OK;
    mutex_lock(master[bus_info->port].bus_lock);
    ret = i2c_master_probe(master[bus_info->port].master_handle, addr >> 1, DEFAULT_I2C_TRANS_TIMEOUT);
    mutex_unlock(master[bus_info->port].bus_lock);
    return ret;
}

esp_err_t i2c_bus_run_cb(i2c_bus_handle_t bus, i2c_run_cb_t cb, void *arg)
{
    I2C_BUS_CHECK(bus != NULL, "Handle error", ESP_FAIL);
    I2C_BUS_CHECK(cb != NULL, "Invalid callback", ESP_FAIL);

    i2c_bus_info_t *bus_info = (i2c_bus_info_t *)bus;
    mutex_lock(master[bus_info->port].bus_lock);
    (*cb)(bus_info->port, arg);
    mutex_unlock(master[bus_info->port].bus_lock);
    return ESP_OK;
}

#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) */
