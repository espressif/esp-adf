Downmix
=======

This API is intended for mixing of two audio files (streams), defined as the base audio file and the newcome audio file, into one output audio file. 

The newcome audio file will be downmixed into the base audio file with individual gains applied to each file.

.. blockdiag::
    :caption: Illustration of Downmixing Process
    :align: center

    blockdiag illustration_of_downmixing_process {

        # global attributes
        default_shape = roundedbox;
        span_width = 40;
        node_width = 80;
        span_height = 30;

        # labels of diagram nodes
        READ_BASE_FILE [label="Read\n Base\n MP3 file", shape=flowchart.input, height=60, width=120];
        MP3_BASE_DECODER [label="MP3\n decoder"];
        I2S_BASE_STREAM [label="I2S\n stream"];
        DOWNMIX [label="Downmix"];
        CODEC_CHIP [label="Codec\n chip", shape=box, height=60];

        READ_NEWCOME_FILE [label="Read\n Newcome\n MP3 file", shape=flowchart.input, height=60, width=120];
        MP3_NEWCOME_DECODER [label="MP3\n decoder"];
        I2S_NEWCOME_STREAM [label="I2S\n stream"];
        KNOT [shape=none]

        # node connections
        READ_BASE_FILE -> MP3_BASE_DECODER -> I2S_BASE_STREAM  -> DOWNMIX -> CODEC_CHIP;
        READ_NEWCOME_FILE -> MP3_NEWCOME_DECODER -> I2S_NEWCOME_STREAM -- KNOT;
        KNOT -> DOWNMIX [folded]
        group {
           label = "Base File"
           color = lightgreen;

           READ_BASE_FILE; MP3_BASE_DECODER; I2S_BASE_STREAM
        }
        group {
           label = "Newcome File"
           color = lightyellow;
           READ_NEWCOME_FILE; MP3_NEWCOME_DECODER; I2S_NEWCOME_STREAM
        }
    }

The number of channel(s) of the output audio file will be the same with that of the base audio file. The number of channel(s) of the newcome audio file will also be changed to the same with the base audio file, if it is different from that of the base audio file.

The downmix process has 3 states:

    * Bypass Downmixing -- Only the base audio file will be processed;

    * Switch on Downmixing -- The base audio file and the target audio file will first enter the transition period, during which the gains of these two files will be changed from the original level to the target level; then enter the stable period, sharing a same target gain;

    * Switch off Downmixing -- The base audio file and the target audio file will first enter the transition period, during which the gains of these two files will be changed back to their original levels; then enter the stable period, with their original gains, respectively. After that, the downmix process enters the bypass state.

Note that, the sample rates of the base audio file and the newcome audio file must be the same, otherwise an error occurs.


Application Example
-------------------

Implementation of this API is demonstrated in :example:`advanced_examples/downmix_pipeline` example.


API Reference
-------------

.. include:: /_build/inc/downmix.inc
