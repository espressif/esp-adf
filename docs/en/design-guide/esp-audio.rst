Audio Player
============

Example
-------

::

    esp_audio_cfg_t cfg = {
        .in_stream_buf_size = 4096,                    /*!< Input buffer size */
        .out_stream_buf_size = 4096,                   /*!< Output buffer size */
        .evt_que = NULL,                               /*!< Registered by uesr for receiving esp_audio event */
        .resample_rate = 48000,                        /*!< sample rate */
        .hal = NULL,                                   /*!<  */
    };
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    cfg.hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    esp_audio_handle_t player = esp_audio_create(&cfg);
    TEST_ASSERT_NOT_EQUAL(player, NULL);
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_READER,
    };
    audio_element_handle_t raw =  raw_stream_init(&raw_cfg);
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    audio_element_handle_t wav = wav_decoder_init(&wav_cfg);

     fatfs_stream_cfg_t fatfs_cfg = {
        .type = AUDIO_STREAM_READER,
        .root_path = "/sdcard",
    };
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    esp_audio_input_stream_add(player, fatfs_stream_init(&fatfs_cfg));

    i2s_cfg.type = AUDIO_STREAM_WRITER;
    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_cfg));
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav);
