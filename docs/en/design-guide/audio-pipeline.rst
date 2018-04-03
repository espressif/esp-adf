Audio Pipeline
==============

Example
-------

::

    audio_element_handle_t first_el, mid_el, last_el;
    audio_element_cfg_t el_cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();

    el_cfg.open = _el_open;
    el_cfg.read = _el_read;
    el_cfg.process = _el_process;
    el_cfg.close = _el_close;
    first_el = audio_element_init(&el_cfg, "first");
    TEST_ASSERT_NOT_NULL(first_el);

    el_cfg.read = NULL;
    el_cfg.write = NULL;
    mid_el = audio_element_init(&el_cfg, "mid");
    TEST_ASSERT_NOT_NULL(mid_el);
    el_cfg.write = _el_write;
    last_el = audio_element_init(&el_cfg, "last");
    TEST_ASSERT_NOT_NULL(last_el);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);
    TEST_ASSERT_NOT_NULL(pipeline);
    TEST_ASSERT_EQUAL(ESP_OK, audio_pipeline_register(pipeline, first_el, mid_el, last_el));
    TEST_ASSERT_EQUAL(ESP_OK, audio_pipeline_link(pipeline, (const char *[]){"first", "mid", "last"}, 3));
