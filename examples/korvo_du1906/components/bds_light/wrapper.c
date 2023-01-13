#include "esp_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"
#include "esp_tls.h"

#ifdef CONFIG_MBEDTLS_ROM_MD5
#include "esp_rom_md5.h"
#undef mbedtls_md5_init
#undef mbedtls_md5_free
#undef mbedtls_md5_update_ret
#undef mbedtls_md5_finish_ret
#undef mbedtls_md5_starts_ret
#endif

#ifdef CONFIG_MBEDTLS_ROM_MD5
void mbedtls_md5_init(mbedtls_md5_context *ctx)
{
    esp_rom_md5_init((md5_context_t*)ctx);
}

void mbedtls_md5_free(mbedtls_md5_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_md5_context));
}
#endif

int mbedtls_md5_starts_ret( mbedtls_md5_context *ctx )
{
    #ifndef CONFIG_MBEDTLS_ROM_MD5
    return mbedtls_md5_starts(ctx);
    #else
    md5_context_t* rom_md5 = ctx;
    esp_rom_md5_init(rom_md5);
    return 0;
    #endif
}

int mbedtls_md5_update_ret( mbedtls_md5_context *ctx,
                        const unsigned char *input,
                        size_t ilen )
{
    #ifndef CONFIG_MBEDTLS_ROM_MD5
    return mbedtls_md5_update(ctx, input, ilen);
    #else
    md5_context_t* rom_md5 = ctx;
    esp_rom_md5_update(rom_md5, input, ilen);
    return 0;
    #endif
}

int mbedtls_md5_finish_ret( mbedtls_md5_context *ctx,
                        unsigned char output[16] )
{
    #ifndef CONFIG_MBEDTLS_ROM_MD5
    return mbedtls_md5_finish(ctx, output);
    #else
    md5_context_t* rom_md5 = ctx;
    esp_rom_md5_final(output, rom_md5);
    return 0;
    #endif
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
int esp_tls_conn_delete(esp_tls_t *tls)
{
    return esp_tls_conn_destroy(tls);
}
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
esp_tls_t* esp_tls_conn_new(const char *hostname, int hostlen, int port, const esp_tls_cfg_t *cfg)
{
    esp_tls_t* tls = esp_tls_init();
    if (esp_tls_conn_new_sync(hostname, strlen(hostname), port, cfg, tls) <= 0) {
        esp_tls_conn_destroy(tls);
        tls = NULL;
    }
    return tls;
}
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
int mbedtls_sha1_ret( const unsigned char *input,
                  size_t ilen,
                  unsigned char output[20])
{
    return mbedtls_sha1(input, ilen, output);
}
#endif

#endif
