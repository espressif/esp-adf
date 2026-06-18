
set(priv_requires
        unity
        driver
        esp_driver_i2c
        esp_driver_i2s
        esp_pm
        esp_codec_dev
)

idf_component_register(SRC_DIRS .
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES ${priv_requires}
                       WHOLE_ARCHIVE TRUE
                       )
