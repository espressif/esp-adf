*************
API Reference
*************

This API provides a way to develop audio applications using :doc:`Elements <framework/audio_element>` like :doc:`Codecs <codecs/index>` (Decoders and Encoders), :doc:`Streams <streams/index>` or :doc:`Audio Processing <audio-processing/index>` functions.

.. blockdiag::
    :caption: **Elements** of the Audio Development Framework
    :align: center

    blockdiag audio_elements {

        # global attributes
        default_shape = roundedbox;
        node_width = 100;
        node_height = 140;
        span_width = 40;
        span_height = 15;

        # labels of diagram nodes
        STREAMS [label="I2S\n HTTP\n FatFs\n Raw\n Spifss"];
        DECODERS [label="AAC\n AMR\n FLAC\n MP3\n OGG\n OPUS\n WAV"];
        ENCODERS [label="AMRNB\n AMRWB\n WAV"];
        AUDIO_PROC [label="Downmix\n Equalizer\n Resample Filter\n Sonic", width=140];

        # labels of description nodes
        STREAMS_DESC [label=Streams, height=30, shape=note, color=yellow];
        DECODERS_DESC  [label=Decoders, height=30, shape=note, color=yellow];
        ENCODERS_DESC  [label=Encoders, height=30, shape=note, color=yellow];
        AUDIO_PROC_DESC [label="Audo Processing", height=30, width=140, shape=note, color=yellow];
        STREAMS -- DECODERS -- ENCODERS -- AUDIO_PROC  [style=none];
        STREAMS_DESC -- DECODERS_DESC -- ENCODERS_DESC -- AUDIO_PROC_DESC [style=none]
    }

The application is developed by combining the :doc:`Elements <framework/audio_element>` into a :doc:`Pipeline <framework/audio_pipeline>`. A diagram below presents organization of two elements, MP3 decoder and I2S stream, in the Audio Pipeline, that has been used in :example:`get-started/play_mp3` example.

.. blockdiag::
    :caption: Sample Organization of Elements in Audio Pipeline
    :align: center

    blockdiag audio_pipeline_example_play_mp3 {

        # global attributes
        default_shape = roundedbox;
        default_group_color = lightgrey;
        span_width = 40;
        node_width = 80;
        span_height = 30;

        # labels of diagram nodes
        READ_FILE [label="Read\n MP3\n file", shape=flowchart.input, height=60, width=100];
        MP3_DECODER [label="MP3\n decoder"];
        I2S_STREAM [label="I2S\n stream"];
        CODEC_CHIP [label="Codec\n chip", shape=box, height=60];

        # node connections
        group {
            label = "Audio Pipeline";
            READ_FILE -> MP3_DECODER -> I2S_STREAM -> CODEC_CHIP;
        }
    }


The audio data is typically acquired using an input :doc:`Stream <streams/index>`, processed with :doc:`Codecs <codecs/index>` and in some cases with :doc:`Audio Processing <audio-processing/index>` functions, and finally output with another :doc:`Stream <streams/index>`. There is an :doc:`Event Interface <framework/audio_event_iface>` to facilitate communication of the application events. Interfacing with specific hardware is done using :doc:`Peripherals <peripherals/index>`.

See a table of contents below with links to description of all the above components.

.. toctree::
    :caption: Table of Contents
    :maxdepth: 2

    Framework <framework/index>
    Streams <streams/index>
    Codecs <codecs/index>
    Audio Processing <audio-processing/index>
    Services <services/index>
    Speech Recognition <speech-recognition/index>
    Peripherals <peripherals/index>
    Abstraction Layer <abstraction/index>
    Configuration Options <kconfig>
