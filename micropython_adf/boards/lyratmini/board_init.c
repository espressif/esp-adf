#include "board_init.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "i2c_bus.h"

#define ES7243E_ADDR 0x20
#define ES7243_ADDR  0x26

static char *TAG = "LyraTMini_board";
static audio_hal_handle_t audio_hal = NULL;

static int audio_board_probe_adc(uint8_t addr)
{
    static i2c_bus_handle_t i2c_handle = NULL;

    if (i2c_handle == NULL) {
        i2c_config_t es_i2c_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,
        };
        es_i2c_cfg.sda_io_num = 18;
        es_i2c_cfg.scl_io_num = 23;
        i2c_handle = i2c_bus_create(I2C_NUM_0, &es_i2c_cfg);
    }

    if (i2c_handle) {
        return i2c_bus_probe_addr(i2c_handle, addr);
    }
    return ESP_FAIL;
}

audio_hal_handle_t board_codec_init(void)
{
    if (audio_hal) {
        return audio_hal;
    }
    ESP_LOGI(TAG, "init");
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    audio_hal_ctrl_codec(audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    // The new version of the esp32_lyrat_mini development board has replaced the ADC from ES7243 to ES7243E.
    // Check the current ADC in use and initialize it.
    audio_hal_handle_t adc_hal = NULL;
    if (audio_board_probe_adc(ES7243E_ADDR) == ESP_OK) {
        audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES7243E_DEFAULT_HANDLE);
    } else if (audio_board_probe_adc(ES7243_ADDR) == ESP_OK) {
        audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES7243_DEFAULT_HANDLE);
    }
    audio_hal_ctrl_codec(adc_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    return audio_hal;
}
