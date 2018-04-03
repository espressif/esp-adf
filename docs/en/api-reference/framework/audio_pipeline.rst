Audio Pipeline
==============

Dynamic combination of a group of linked :doc:`Elements <audio_element>` is done using the Audio Pipeline. You do not deal with the individual elements but with just one audio pipeline. Every element is connected by a ringbuffer. The Audio Pipeline also takes care of forwarding messages from the element tasks to an application.

A diagram below presents organization of three elements, HTTP reader stream, MP3 decoder and I2S writer stream, in the Audio Pipeline, that has been used in :example:`player/pipeline_http_mp3` example.

.. blockdiag::
    :caption: Sample Organization of Elements in Audio Pipeline
    :align: center

    blockdiag audio_pipeline_example_pipeline_http_mp3 {

        # global attributes
        default_shape = roundedbox;
        default_group_color = lightgrey;
        node_height = 60;
        node_width = 80;
        span_width = 40;
        span_height = 30;

        # labels of diagram nodes
        SERVER [label="http://..\n server", shape=flowchart.input, height=70, width=120];
        HTTP_READER [label="HTTP\n reader\n stream"];
        MP3_DECODER [label="MP3\n decoder"];
        I2S_STREAM [label="I2S\n writer\n stream"];
        CODEC_CHIP [label="Codec\n chip", shape=box, height=70];

        # node connections
        group {
            label = "Audio Pipeline";
            SERVER -> HTTP_READER -> MP3_DECODER -> I2S_STREAM -> CODEC_CHIP;
        }
    }

API Reference
-------------

.. include:: /_build/inc/audio_pipeline.inc
