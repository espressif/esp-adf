#include <string.h>
#include "esp_log.h"
#include "tlv320_reg.h"
#include "tlv320_codec.h"
#include "esp_codec_dev_vol.h"

#define TAG "TLV320_CODEC"

typedef struct {
    audio_codec_if_t base;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    bool is_open;
    bool enabled;
    esp_codec_dec_work_mode_t codec_mode;
    int16_t pa_pin;
    bool pa_reverted;
    float hw_gain;
} audio_codec_tlv320_t;

static const esp_codec_dev_vol_range_t vol_range = {
    .min_vol = { .vol = 0x7F, .db_value = -63.5f },
    .max_vol = { .vol = 0x00, .db_value = 0.0f },
};

static int tlv320_write_reg(audio_codec_tlv320_t *codec, uint8_t reg, uint8_t val) {
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &val, 1);
}

static int tlv320_read_reg(audio_codec_tlv320_t *codec, uint8_t reg, int *val) {
    uint8_t temp = 0;
    int res = codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, &temp, 1);
    *val = temp;
    return res;
}

static int tlv320_set_volume(audio_codec_tlv320_t *codec, float db) {
    db -= codec->hw_gain;
    int volume = esp_codec_dev_vol_calc_reg(&vol_range, db);
    // tlv320_write_reg(codec, TLV320_REG_DAC_VOL_LEFT, volume);
    // tlv320_write_reg(codec, TLV320_REG_DAC_VOL_RIGHT, volume);
    return ESP_OK;
}

static int tlv320_set_mute(audio_codec_tlv320_t *codec, bool mute) {
    // int reg = 0;
    // tlv320_read_reg(codec, TLV320_REG_DAC_MUTE, &reg);
    // reg = mute ? (reg | 0x0C) : (reg & ~0x0C);
    // tlv320_write_reg(codec, TLV320_REG_DAC_MUTE, reg);
    return ESP_OK;
}

static int tlv320_configure(audio_codec_tlv320_t *codec) {
    int res = 0;

    // // Set to page 0
    // res |= tlv320_write_reg(codec, TLV320_REG_PAGE_SELECT, 0x00);
    // res |= tlv320_write_reg(codec, TLV320_REG_SOFTWARE_RESET, 0x01);

    // // vTaskDelay(pdMS_TO_TICKS(10));

    // // Configure PLL and clock
    // res |= tlv320_write_reg(codec, TLV320_REG_CLK_GEN_MUX, 0x01); // PLL
    // res |= tlv320_write_reg(codec, TLV320_REG_PLL_P_R, 0x91);     // P=1, R=1
    // res |= tlv320_write_reg(codec, TLV320_REG_PLL_J, 0x08);       // J=8
    // res |= tlv320_write_reg(codec, TLV320_REG_PLL_D_MSB, 0x00);
    // res |= tlv320_write_reg(codec, TLV320_REG_PLL_D_LSB, 0x00);

    // res |= tlv320_write_reg(codec, TLV320_REG_NDAC, 0x81); // NDAC = 1
    // res |= tlv320_write_reg(codec, TLV320_REG_MDAC, 0x81); // MDAC = 1
    // res |= tlv320_write_reg(codec, TLV320_REG_DOSR_MSB, 0x00);
    // res |= tlv320_write_reg(codec, TLV320_REG_DOSR_LSB, 0x80); // DOSR = 128

    // // Audio Interface config (I2S, 16-bit)
    // res |= tlv320_write_reg(codec, TLV320_REG_IFACE_CTRL_1, 0x00); // I2S, 16bit
    // res |= tlv320_write_reg(codec, TLV320_REG_IFACE_CTRL_2, 0x00);
    // res |= tlv320_write_reg(codec, TLV320_REG_IFACE_CTRL_3, 0x00);

    // // Power-up DAC
    // res |= tlv320_write_reg(codec, TLV320_REG_DAC_PWR_CTRL, 0xD4); // Enable L/R DAC
    // res |= tlv320_write_reg(codec, TLV320_REG_DAC_MUTE, 0x00);     // Unmute
    // res |= tlv320_set_volume(codec, 0); // 0 dB volume

    // // Power-up ADC
    // res |= tlv320_write_reg(codec, TLV320_REG_ADC_PWR_CTRL, 0xC0);
    // res |= tlv320_write_reg(codec, TLV320_REG_ADC_VOL_LEFT, 0x80);  // 0dB
    // res |= tlv320_write_reg(codec, TLV320_REG_ADC_VOL_RIGHT, 0x80); // 0dB

    // // DAC Routing
    // res |= tlv320_write_reg(codec, TLV320_REG_PAGE_SELECT, 0x01);
    // res |= tlv320_write_reg(codec, TLV320_PAGE1_DAC_ROUTING_LEFT, 0x08);
    // res |= tlv320_write_reg(codec, TLV320_PAGE1_DAC_ROUTING_RIGHT, 0x08);
    // res |= tlv320_write_reg(codec, TLV320_PAGE1_HP_DRIVER_LEFT, 0x0C);
    // res |= tlv320_write_reg(codec, TLV320_PAGE1_HP_DRIVER_RIGHT, 0x0C);
    // res |= tlv320_write_reg(codec, TLV320_PAGE1_OUTPUT_DRV_PWR_CTRL, 0xC0);

    // // Return to Page 0
    // res |= tlv320_write_reg(codec, TLV320_REG_PAGE_SELECT, 0x00);

    return res;
}

static int tlv320_open(const audio_codec_if_t *h, void *cfg, int cfg_size) {
    // audio_codec_tlv320_t *codec = (audio_codec_tlv320_t *) h;
    // tlv320_codec_cfg_t *codec_cfg = (tlv320_codec_cfg_t *) cfg;

    // if (!codec || !codec_cfg || cfg_size != sizeof(tlv320_codec_cfg_t)) return -1;

    // codec->ctrl_if = codec_cfg->ctrl_if;
    // codec->gpio_if = codec_cfg->gpio_if;
    // codec->pa_pin = codec_cfg->pa_pin;
    // codec->pa_reverted = codec_cfg->pa_reverted;
    // codec->codec_mode = codec_cfg->codec_mode;

    // return tlv320_configure(codec);

    return ESP_OK;
}

static int tlv320_enable(const audio_codec_if_t *h, bool enable) {
    // audio_codec_tlv320_t *codec = (audio_codec_tlv320_t *) h;
    // codec->enabled = enable;
    return ESP_OK;
}

static int tlv320_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs) {
    // For now fixed sample rate. Extendable.
    return ESP_OK;
}

static int tlv320_set_vol(const audio_codec_if_t *h, float db) {
    // return tlv320_set_volume((audio_codec_tlv320_t *) h, db);
    return ESP_OK;
}

static int tlv320_mute(const audio_codec_if_t *h, bool mute) {
    // return tlv320_set_mute((audio_codec_tlv320_t *) h, mute);
    return ESP_OK;
}

static int tlv320_close(const audio_codec_if_t *h) {
    return ESP_OK;
}

static void tlv320_dump(const audio_codec_if_t *h) {
    // audio_codec_tlv320_t *codec = (audio_codec_tlv320_t *) h;
    // for (int i = 0; i <= 0x7F; ++i) {
    //     int val = 0;
    //     tlv320_read_reg(codec, i, &val);
    //     ESP_LOGI(TAG, "Reg 0x%02X = 0x%02X", i, val);
    // }
}

const audio_codec_if_t *tlv320_codec_new(tlv320_codec_cfg_t *cfg) {
    // if (!cfg || !cfg->ctrl_if) return NULL;
    audio_codec_tlv320_t *codec = calloc(1, sizeof(audio_codec_tlv320_t));
    // if (!codec) return NULL;

    // codec->base.open = tlv320_open;
    // codec->base.enable = tlv320_enable;
    // codec->base.set_fs = tlv320_set_fs;
    // codec->base.set_vol = tlv320_set_vol;
    // codec->base.mute = tlv320_mute;
    // codec->base.dump_reg = tlv320_dump;
    // codec->base.close = tlv320_close;
    // codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&cfg->hw_gain);

    // if (codec->base.open(&codec->base, cfg, sizeof(tlv320_codec_cfg_t)) != 0) {
    //     free(codec);
    //     return NULL;
    // }

    return &codec->base;
}
