#ifndef __AUDIO_TONEURI_H__
#define __AUDIO_TONEURI_H__

extern const char* tone_uri[];

typedef enum {
    TONE_TYPE_BT_RECONNECT,
    TONE_TYPE_WECHAT,
    TONE_TYPE_WELCOME_TO_WIFI,
    TONE_TYPE_NEW_VERSION_AVAILABLE,
    TONE_TYPE_BT_SUCCESS,
    TONE_TYPE_FREETALK,
    TONE_TYPE_UPGRADE_DONE,
    TONE_TYPE_SHUTDOWN,
    TONE_TYPE_ALARM,
    TONE_TYPE_WIFI_SUCCESS,
    TONE_TYPE_MAX,
} tone_type_t;

int get_tone_uri_num();

#endif
