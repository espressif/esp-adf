Audio Element
=============

Example
-------


::

    audio_element_handle_t el;
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _el_open;
    cfg.read = _el_read;
    cfg.process = _el_process;
    cfg.write = _el_write;
    cfg.close = _el_close;
    el = audio_element_init(&cfg);
    TEST_ASSERT_NOT_NULL(el);
    TEST_ASSERT_EQUAL(ESP_OK, audio_element_start(el));
