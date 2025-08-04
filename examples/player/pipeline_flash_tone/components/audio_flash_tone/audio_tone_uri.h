#ifndef __AUDIO_TONEURI_H__
#define __AUDIO_TONEURI_H__

extern const char* tone_uri[];

typedef enum {
    TONE_TYPE_BT_CONNECT,
    TONE_TYPE_BT_FAIL,
    TONE_TYPE_BT_SUCCESS,
    TONE_TYPE_MIC_CLOSE,
    TONE_TYPE_MIC_OPEN,
    TONE_TYPE_SERVER_CONNECT,
    TONE_TYPE_UPGRADE_DONE,
    TONE_TYPE_UPGRADE_FAILED,
    TONE_TYPE_UPGRADING,
    TONE_TYPE_WIFI_CONNECT,
    TONE_TYPE_MAX,
} tone_type_t;

int get_tone_uri_num();

#endif
