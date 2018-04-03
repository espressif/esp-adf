Audio Event
===========

Example
-------

::

    audio_event_handle_t evt1;
    audio_event_cfg_t cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    cfg.dispatcher = evt_process;
    cfg.queue_size = 10;
    cfg.context = &evt1;
    cfg.type = AUDIO_EVENT_TYPE_ELEMENT;
    evt1 = audio_event_init(&cfg);
    TEST_ASSERT_NOT_NULL(evt1);

    audio_event_msg_t msg;
    int i;
    ESP_LOGI(TAG, "[✓] dispatch 10 msg to evt1");
    for (i = 0; i < 10; i++) {
        msg.cmd = i;
        TEST_ASSERT_EQUAL(ESP_OK, audio_event_dispatch(evt1, &msg));
    }
    msg.cmd = 10;
    TEST_ASSERT_EQUAL(ESP_FAIL, audio_event_dispatch(evt1, &msg));
    ESP_LOGI(TAG, "[✓] listening 10 event have dispatched fron evt1");
    while (audio_event_listen(evt1) == ESP_OK);
