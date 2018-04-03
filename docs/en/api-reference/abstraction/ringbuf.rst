Ring Buffer
===========

Ringbuffer is designed in addition to use as a data buffer, also used to connect Audio Elements. Each Element that requests data from the Ringbuffer will block the task until the data is available. Or block the task when writing data and the Buffer is full. Of course, we can stop this block at any time.

.. blockdiag::
    :caption: Ring Buffer used in Audio Pipeline
    :align: center
    
    blockdiag {
        default_shape = roundedbox;
        default_group_color = lightgrey;
        node_height = 60; 
        node_width = 80; 
        span_width = 40;
        span_height = 30;

        input [shape = roundedbox, color = lightgray, label="File\nReader"];
        ringbuf_1 [shape = flowchart.database, label="Ring\nbuffer"];
        process [shape = roundedbox, color = lightgray, label="MP3\nDecoder"];
        ringbuf_2 [shape = flowchart.database, label="Ring\nbuffer"];
        output [shape = roundedbox, color = lightgray, label="I2S\nWriter"];
        input -> ringbuf_1 -> process -> ringbuf_2 -> output
    }


Application Example
-------------------

In most of ESP-ADF :adf:`examples` connecting of Elements with Ringbuffers is done "behind the scenes" by a function :cpp:func:`audio_pipeline_link`. To see this operation exposed check :example:`player/element_sdcard_mp3` example.


API Reference
-------------

.. include:: /_build/inc/ringbuf.inc
