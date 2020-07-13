#ifndef __AUDIO_TONEURI_H__
#define __AUDIO_TONEURI_H__

extern const char* tone_uri[];

typedef enum {
    TONE_TYPE_ALREADY_NEW,
    TONE_TYPE_BOOT,
    TONE_TYPE_BT_CONNECT,
    TONE_TYPE_BT_DISCONNECT,
    TONE_TYPE_DOWNLOADED,
    TONE_TYPE_LINKED,
    TONE_TYPE_NOT_FIND,
    TONE_TYPE_OTA_FAIL,
    TONE_TYPE_OTA_START,
    TONE_TYPE_SHUT_DOWN,
    TONE_TYPE_SMART_CONFIG,
    TONE_TYPE_UNLINKED,
    TONE_TYPE_WAKEUP,
    TONE_TYPE_MAX,
} tone_type_t;

int get_tone_uri_num();

#endif
