Software Design
***************

Espressif audio framework project.

Features
========

1.  All of Streams and Codecs based on audio element.
2.  All events based on queue.
3.  Audio pipeline supports dynamic combination.
4.  Audio pipeline supports multiple elements.
5.  Pipeline Support functionality plug-in.
6.  Audio common peripherals support work in the one task.
7.  Support post-event mechanism in peripherals.
8.  Support high level audio play API based on element and audio pipeline.
9.  Audio high level interface supports dynamic adding of codec library.
10. Audio high level interface supports dynamic adding of input and output stream.
11. ESP audio supports multiple audio pipelines.

Design Components
=================

Five basic components are - Audio Element, Audio Event, Audio Pipeline, ESP peripherals, ESP audio

.. toctree::
   :maxdepth: 1

    Audio Element <audio-element>
    Audio Event <audio-event>
    Audio Pipeline <audio-pipeline>
    Audio Peripherals <audio-peripheral>
    Audio Player <esp-audio>

