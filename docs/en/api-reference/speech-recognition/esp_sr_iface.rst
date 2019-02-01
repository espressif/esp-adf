Speech Recognition Interface
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

A code snippet below demonstrates how to initialize the model, determine the number of samples and the sample rate of voice data to feed to the model, and detect the wakeup word.

.. code-block:: c

    #include "esp_sr_iface.h"
    #include "esp_sr_models.h"

    static const sr_model_iface_t *model = &sr_model_wakenet3_quantized;

    // Initialize wakeNet model data
    static model_iface_data_t *model_data = model->create(DET_MODE_90);

    // Set parameters of buffer
    int audio_chunksize = model->get_samp_chunksize(model_data);
    int frequency = model->get_samp_rate(model_data);
    int16_t *buffer = malloc(audio_chunksize sizeof(int16_t));

    // Get voice data feed to buffer
    ...

    // Detect
    int r = model->detect(model_data, buffer);
    if (r > 0) {
        printf("Detection triggered output %d.\n",  r);
    }
    
    // Destroy model
    model->destroy(model_data)


Application Example
-------------------

Implementation of the speech recognition API is demonstrated in :example:`speech_recognition/asr` example.


API Reference
-------------

.. include:: /_build/inc/esp_sr_iface.inc

