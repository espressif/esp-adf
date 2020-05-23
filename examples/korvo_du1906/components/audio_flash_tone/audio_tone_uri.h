#ifndef __AUDIO_TONEURI_H__
#define __AUDIO_TONEURI_H__

extern const char* tone_uri[];

typedef enum {
    TONE_TYPE_LINKED,
    TONE_TYPE_NOT_FIND,
    TONE_TYPE_SMART_CONFIG,
    TONE_TYPE_UNLINKED,
    TONE_TYPE_WAKEUP,
    TONE_TYPE_MAX,
} tone_type_t;

int get_tone_uri_num();

#endif
