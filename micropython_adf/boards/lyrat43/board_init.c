#include "board_init.h"
#include "esp_log.h"

static char *TAG = "LyraT43_board";
static audio_hal_handle_t audio_hal = NULL;

audio_hal_handle_t board_codec_init(void)
{
    if (audio_hal) {
        return audio_hal;
    }
    ESP_LOGI(TAG, "init");
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8388_DEFAULT_HANDLE);
    audio_hal_ctrl_codec(audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    return audio_hal;
}
