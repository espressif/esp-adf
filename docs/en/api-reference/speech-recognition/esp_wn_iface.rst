WakeNet Interface
============================

Setting up the speech recognition application to detect a wakeup word may be done using series of Audio Elements linked into a pipeline shown below.

.. blockdiag::
    :caption: Sample Speech Recognition Pipeline
    :align: center

    blockdiag speech_recognition_pipeline {

        # global attributes
        default_shape = roundedbox;
        default_group_color = lightgrey;
        span_width = 40;
        node_width = 80;
        span_height = 30;

        # labels of diagram nodes
        MICROPHONE [label="Mic", shape=beginpoint];
        CODEC_CHIP [label="Codec\n chip"];
        I2S_STREAM [label="I2S\n stream"];
        FILTER [label="Filter"];
        RAW_STREAM [label="RAW\n stream"];
        SPEECH_RECOGNITION [label="Speech\n recognition", shape=box, height=60];

        # node connections
        MICROPHONE -> CODEC_CHIP -> I2S_STREAM -> FILTER -> RAW_STREAM -> SPEECH_RECOGNITION;
    }


Configuration and use of particular elements is demonstrated in several :adf:`examples <examples>` linked to elsewhere in this documentation. What may need clarification is use of the **Filter** and the **RAW stream**. The filter is used to adjust the sample rate of the I2S stream to match the sample rate of the speech recognition model. The RAW stream is the way to feed the audio input to the model.

The above introduction is the primary guidance. ESP-ADF offers users a more flexible and convenient module, namely the :doc:`audio recorder <audio_recorder>`, which is strongly recommended for use.


API Reference
-------------

For the latest speech recognition API reference, please refer to `ESP-SR Speech Recognition Framework <https://github.com/espressif/esp-sr>`_.
