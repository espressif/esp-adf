/* RTMP application test settings 

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "rtmp_app_setting.h"
#include "esp_crt_bundle.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#define TAG "RTMP_Set"

// Set default value of basic settings
static bool use_sw_jpg = false;
static av_record_video_quality_t video_quality = AV_RECORD_VIDEO_QUALITY_QVGA;
static uint8_t video_fps = 8;
static bool allow_running = true;
static uint8_t audio_channel = 1;
static int audio_sample_rate = 11025;
static av_record_audio_fmt_t audio_fmt = AV_RECORD_AUDIO_FMT_AAC;
static av_record_video_fmt_t video_fmt = AV_RECORD_VIDEO_FMT_H264;

static const  char *rtmps_cert_pem = "-----BEGIN CERTIFICATE-----\n" 
"MIIDKzCCAhOgAwIBAgIUBxM3WJf2bP12kAfqhmhhjZWv0ukwDQYJKoZIhvcNAQEL\n"
"BQAwJTEjMCEGA1UEAwwaRVNQMzIgSFRUUFMgc2VydmVyIGV4YW1wbGUwHhcNMTgx\n"
"MDE3MTEzMjU3WhcNMjgxMDE0MTEzMjU3WjAlMSMwIQYDVQQDDBpFU1AzMiBIVFRQ\n"
"UyBzZXJ2ZXIgZXhhbXBsZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
"ALBint6nP77RCQcmKgwPtTsGK0uClxg+LwKJ3WXuye3oqnnjqJCwMEneXzGdG09T\n"
"sA0SyNPwrEgebLCH80an3gWU4pHDdqGHfJQa2jBL290e/5L5MB+6PTs2NKcojK/k\n"
"qcZkn58MWXhDW1NpAnJtjVniK2Ksvr/YIYSbyD+JiEs0MGxEx+kOl9d7hRHJaIzd\n"
"GF/vO2pl295v1qXekAlkgNMtYIVAjUy9CMpqaQBCQRL+BmPSJRkXBsYk8GPnieS4\n"
"sUsp53DsNvCCtWDT6fd9D1v+BB6nDk/FCPKhtjYOwOAZlX4wWNSZpRNr5dfrxKsb\n"
"jAn4PCuR2akdF4G8WLUeDWECAwEAAaNTMFEwHQYDVR0OBBYEFMnmdJKOEepXrHI/\n"
"ivM6mVqJgAX8MB8GA1UdIwQYMBaAFMnmdJKOEepXrHI/ivM6mVqJgAX8MA8GA1Ud\n"
"EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBADiXIGEkSsN0SLSfCF1VNWO3\n"
"emBurfOcDq4EGEaxRKAU0814VEmU87btIDx80+z5Dbf+GGHCPrY7odIkxGNn0DJY\n"
"W1WcF+DOcbiWoUN6DTkAML0SMnp8aGj9ffx3x+qoggT+vGdWVVA4pgwqZT7Ybntx\n"
"bkzcNFW0sqmCv4IN1t4w6L0A87ZwsNwVpre/j6uyBw7s8YoJHDLRFT6g7qgn0tcN\n"
"ZufhNISvgWCVJQy/SZjNBHSpnIdCUSJAeTY2mkM4sGxY0Widk8LnjydxZUSxC3Nl\n"
"hb6pnMh3jRq4h0+5CZielA4/a+TdrNPv/qok67ot/XJdY3qHCCd8O2b14OVq9jo=\n"
"-----END CERTIFICATE-----\n";

static const char *rtmps_key_pem =  "-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwYp7epz++0QkH\n"
"JioMD7U7BitLgpcYPi8Cid1l7snt6Kp546iQsDBJ3l8xnRtPU7ANEsjT8KxIHmyw\n"
"h/NGp94FlOKRw3ahh3yUGtowS9vdHv+S+TAfuj07NjSnKIyv5KnGZJ+fDFl4Q1tT\n"
"aQJybY1Z4itirL6/2CGEm8g/iYhLNDBsRMfpDpfXe4URyWiM3Rhf7ztqZdveb9al\n"
"3pAJZIDTLWCFQI1MvQjKamkAQkES/gZj0iUZFwbGJPBj54nkuLFLKedw7DbwgrVg\n"
"0+n3fQ9b/gQepw5PxQjyobY2DsDgGZV+MFjUmaUTa+XX68SrG4wJ+DwrkdmpHReB\n"
"vFi1Hg1hAgMBAAECggEAaTCnZkl/7qBjLexIryC/CBBJyaJ70W1kQ7NMYfniWwui\n"
"f0aRxJgOdD81rjTvkINsPp+xPRQO6oOadjzdjImYEuQTqrJTEUnntbu924eh+2D9\n"
"Mf2CAanj0mglRnscS9mmljZ0KzoGMX6Z/EhnuS40WiJTlWlH6MlQU/FDnwC6U34y\n"
"JKy6/jGryfsx+kGU/NRvKSru6JYJWt5v7sOrymHWD62IT59h3blOiP8GMtYKeQlX\n"
"49om9Mo1VTIFASY3lrxmexbY+6FG8YO+tfIe0tTAiGrkb9Pz6tYbaj9FjEWOv4Vc\n"
"+3VMBUVdGJjgqvE8fx+/+mHo4Rg69BUPfPSrpEg7sQKBgQDlL85G04VZgrNZgOx6\n"
"pTlCCl/NkfNb1OYa0BELqWINoWaWQHnm6lX8YjrUjwRpBF5s7mFhguFjUjp/NW6D\n"
"0EEg5BmO0ePJ3dLKSeOA7gMo7y7kAcD/YGToqAaGljkBI+IAWK5Su5yldrECTQKG\n"
"YnMKyQ1MWUfCYEwHtPvFvE5aPwKBgQDFBWXekpxHIvt/B41Cl/TftAzE7/f58JjV\n"
"MFo/JCh9TDcH6N5TMTRS1/iQrv5M6kJSSrHnq8pqDXOwfHLwxetpk9tr937VRzoL\n"
"CuG1Ar7c1AO6ujNnAEmUVC2DppL/ck5mRPWK/kgLwZSaNcZf8sydRgphsW1ogJin\n"
"7g0nGbFwXwKBgQCPoZY07Pr1TeP4g8OwWTu5F6dSvdU2CAbtZthH5q98u1n/cAj1\n"
"noak1Srpa3foGMTUn9CHu+5kwHPIpUPNeAZZBpq91uxa5pnkDMp3UrLIRJ2uZyr8\n"
"4PxcknEEh8DR5hsM/IbDcrCJQglM19ZtQeW3LKkY4BsIxjDf45ymH407IQKBgE/g\n"
"Ul6cPfOxQRlNLH4VMVgInSyyxWx1mODFy7DRrgCuh5kTVh+QUVBM8x9lcwAn8V9/\n"
"nQT55wR8E603pznqY/jX0xvAqZE6YVPcw4kpZcwNwL1RhEl8GliikBlRzUL3SsW3\n"
"q30AfqEViHPE3XpE66PPo6Hb1ymJCVr77iUuC3wtAoGBAIBrOGunv1qZMfqmwAY2\n"
"lxlzRgxgSiaev0lTNxDzZkmU/u3dgdTwJ5DDANqPwJc6b8SGYTp9rQ0mbgVHnhIB\n"
"jcJQBQkTfq6Z0H6OoTVi7dPs3ibQJFrtkoyvYAbyk36quBmNRjVh6rc8468bhXYr\n"
"v/t+MeGJP/0Zw8v/X2CFll96\n"
"-----END PRIVATE KEY-----\n";

static bool allow_insecure = false;

#define SET_FUNCTION_CONSTRUCT(func_name, arg_type, dst) \
void func_name(arg_type setting)                         \
{                                                        \
    dst = setting;                                       \
}

#define GET_FUNCTION_CONSTRUCT(func_name, arg_type, from) \
arg_type func_name()                                      \
{                                                         \
    return from;                                          \
} 

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_allow_run, bool, allow_running)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_allow_run, bool, allow_running)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_sw_jpeg, bool, use_sw_jpg)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_sw_jpeg, bool, use_sw_jpg)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_fps, uint8_t, video_fps)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_fps, uint8_t, video_fps)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_quality, av_record_video_quality_t, video_quality)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_quality, av_record_video_quality_t, video_quality)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_fmt, av_record_audio_fmt_t, audio_fmt)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_fmt, av_record_audio_fmt_t, audio_fmt)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_fmt, av_record_video_fmt_t, video_fmt)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_fmt, av_record_video_fmt_t, video_fmt)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_channel, uint8_t, audio_channel)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_channel, uint8_t, audio_channel)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_sample_rate, int, audio_sample_rate)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_sample_rate, int, audio_sample_rate)
SET_FUNCTION_CONSTRUCT(rtmp_setting_set_allow_insecure, bool, allow_insecure)

void rtmp_setting_get_server_ssl_cfg(media_lib_tls_server_cfg_t *ssl_cfg)
{
    memset(ssl_cfg, 0, sizeof(media_lib_tls_server_cfg_t));
    ssl_cfg->servercert_buf = rtmps_cert_pem;
    ssl_cfg->servercert_bytes = strlen(rtmps_cert_pem) + 1;
    ssl_cfg->serverkey_buf = rtmps_key_pem; 
    ssl_cfg->serverkey_bytes = strlen(rtmps_key_pem) + 1;
}

void rtmp_setting_get_client_ssl_cfg(const char *url, media_lib_tls_cfg_t *ssl_cfg)
{
    memset(ssl_cfg, 0, sizeof(media_lib_tls_cfg_t));
    if (allow_insecure) {
        return;
    }
    if (strstr(url, "127.0.0.1")) {
        ssl_cfg->cacert_buf = rtmps_cert_pem;
        ssl_cfg->cacert_bytes =  strlen(rtmps_cert_pem) + 1;
        ESP_LOGI(TAG, "Use local CA");
    } else {
        #if  (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
        #if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        ssl_cfg->crt_bundle_attach = esp_crt_bundle_attach;
        ESP_LOGI(TAG, "Use Bundle");
        #else
        ESP_LOGE(TAG, "Need enable CONFIG_MBEDTLS_CERTIFICATE_BUNDLE");
        #endif
        #endif
    }
    ssl_cfg->skip_common_name = true;
}
