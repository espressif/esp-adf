# ADF Detail Design Docs
Espressif audio framework project.

## Features
1. All of Streams and Codecs based on audio element.
2. All of event based on queue.
3. Audio pipeline support dynamic combine.
4. Audio pipeline support multiple element.
5. Support functionality plug-in.
6. Support multiple audio pipeline.

## Design Components
Three basic components,Audio Element, Audio Event and Audio Pipeline.

### Audio_Element
The most important object in ADF for the application programmer is the audio_element object. Every decoder, encoder, filter, input stream, or output stream is in fact a audio_element.

![Audio Element](../_static/element_user.jpg "Audio Element")

### Audio_Event
Events contain error message, control information, element status and end-of-stream notifiers.
Audio event can be dispatch message between audio_element and audio_pipeline, subscribe message from application.

### Audio_Pipeline
Audio pipeline allow user to dynamic combine a group of linked elements into one pipeline. You do not deal with the individual elements anymore but with just one audio pipeline.Every element connect by ringbuffer. It's takes care of forwarding messages from the element tasks to an application. 

![Audio Pipeline](../_static/audio_pipeline.jpg "Audio Pipeline")

## Examples
### Audio_Element
```c
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
```

### Audio_Event
```c
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
```

### Audio_Pipeline
```c
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
```