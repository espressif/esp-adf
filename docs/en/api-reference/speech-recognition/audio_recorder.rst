Audio Recorder
===============

The Audio Recorder API is a set of functions to facilitate voice recording. It combines two important functions, namely Audio Front End (AFE) and audio encoding. This allows users to customize AFE's Voice Activity Detection (VAD), Automatic Gain Control (AGC), and Acoustic Echo Cancellation (AEC) settings. The encoding function is used by users to establish the encoding audio element, which supports various formats such as AAC, AMR-NB, AMR-WB, ADPCM, WAV, OPUS, and G711. The `audio_rec_evt_t` event makes it easy for users to interact with the Audio Recorder software.

The data path of the Audio recorder is presented in the diagram below.

.. blockdiag::
    :caption: Audio recorder data path
    :align: center

    blockdiag audio_recorder_data_path {

        # global attributes
        default_shape = roundedbox;
        default_group_color = lightgrey;
        span_width = 40;
        node_width = 65;
        span_height = 30;

        # labels of diagram nodes
        MICROPHONE [label="Mic", shape=beginpoint];
        I2S_STREAM [label="I2S\n stream"];
        RESAMPLE [label="Resample\n element", shape=flowchart.input, width=120];
        RAW_STREAM [label="RAW\n stream"];
        AFE [label="AFE", shape=box];
        WAKENET [label="WakeNet", shape=box];
        MULTINET [label="MultiNet", shape=box];

        ENC_RESAMPLE [label="Resample\n element", shape=flowchart.input, width=120];
        ENCODER [label="Encoder\n element", shape=flowchart.input, width=120];

        # node connections
        MICROPHONE -> I2S_STREAM -> RESAMPLE -> RAW_STREAM -> AFE -> WAKENET -> MULTINET -> ENC_RESAMPLE -> ENCODER;

        RAW_STREAM -> AFE [folded];
    }

The area represented by the parallelogram is configurable by the user according to their needs, such as sampling frequency, whether to encode, and encoding format.



Application Example
-------------------
The :example:`speech_recognition/wwe/` example demonstrates how to initialize the speech recognition model, determine the number of samples and the sample rate of voice data to feed to the model, detect the wake-up word and command words, and encode voice to specific audio format.

API Reference
-------------

.. include:: /_build/inc/audio_recorder.inc
.. include:: /_build/inc/recorder_sr.inc
.. include:: /_build/inc/recorder_encoder.inc

