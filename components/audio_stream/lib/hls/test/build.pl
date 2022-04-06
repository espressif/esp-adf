#!/usr/bin/perl
my @f = <../*.c>;
gen_fake_header();
`gcc @f test.c -I../include -I../ -g -o ./test`;
clear_up();

sub clear_up {
   unlink("../include/audio_mem.h");
   unlink("../include/audio_error.h");
   unlink("../include/esp_log.h");
}

sub gen_fake_header {
    my $audio_mem =<< 'MEM_H';
#include <string.h>
#include <stdlib.h>
#define audio_malloc  malloc
#define audio_free    free
#define audio_strdup  strdup
#define audio_calloc  calloc
#define audio_realloc realloc
MEM_H

    my $audio_error =<< 'ERROR_H';
#include "esp_log.h"
#define AUDIO_CHECK(TAG, a, action, msg) if (!(a)) {                                \
        ESP_LOGE(TAG,"%s:%d (%s): %s", __FILE__, __LINE__, __FUNCTION__, msg);  \
        action;                                                                     \
        }
#define AUDIO_MEM_CHECK(TAG, a, action)  AUDIO_CHECK(TAG, a, action, "Memory exhausted")
ERROR_H

   my $esp_log = << 'ESP_LOG_H';
#include <stdarg.h>
#define LOGOUT(tag, format, ...) printf("%s: "format, tag, ##__VA_ARGS__);
#define ESP_LOGI LOGOUT
#define ESP_LOGE LOGOUT
#define ESP_LOGD LOGOUT
#define ESP_LOGW LOGOUT
ESP_LOG_H

    write_file("../include/audio_mem.h", $audio_mem);
    write_file("../include/audio_error.h", $audio_error);
    write_file("../include/esp_log.h", $esp_log);
}

sub write_file {
    my ($f, $str) = @_;
    open(my $H, '+>', $f) || die "";
    print $H $str;
    close $H;
}
