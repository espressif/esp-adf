
set(priv_requires
        unity
        driver
        esp_codec_dev
)

idf_component_register(SRC_DIRS .
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES unity esp_codec_dev esp_adc
                       PRIV_REQUIRES ${priv_requires}
                       WHOLE_ARCHIVE TRUE
                       )
