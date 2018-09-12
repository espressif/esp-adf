Audio Samples
*************

Music files in this section are intended for testing of audio applications. The files are organized into different `Formats`_ and `Sample Rates`_.

Formats
=======

The table below provides an audio file converted from 'wav' format into several other audio formats.

Two Channel Audio
-----------------

.. csv-table::
    :header: Format, Audio File, Size [kB]
    :widths: 10, 25, 10

    aac, `ff-16b-2c-44100hz.aac <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac>`_, "2,995"
    ac3, `ff-16b-2c-44100hz.ac3 <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.ac3>`_, "2,994"
    aiff, `ff-16b-2c-44100hz.aiff <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aiff>`_, "33,002"
    flac, `ff-16b-2c-44100hz.flac <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.flac>`_, "22,406"
    m4a, `ff-16b-2c-44100hz.m4a <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.m4a>`_, "3,028"
    mp3, `ff-16b-2c-44100hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3>`_, "2,994"
    mp4, `ff-16b-2c-44100hz.mp4 <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp4>`_, "3,079"
    ogg, `ff-16b-2c-44100hz.ogg <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.ogg>`_, "2,612"
    opus, `ff-16b-2c-44100hz.opus <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.opus>`_, "2,598"
    wav, `ff-16b-2c-44100hz.wav <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.wav>`_, "49,504"
    wma, `ff-16b-2c-44100hz.wma <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.wma>`_, "3,227"

Playlist containing all above files: `ff-16b-2c-playlist.m3u <https://dl.espressif.com/dl/audio/ff-16b-2c-playlist.m3u>`_

Single Channel Audio
--------------------

.. csv-table::
    :header: Format, Audio File, Size [kB]
    :widths: 10, 25, 10

    aac, `ff-16b-1c-44100hz.aac <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.aac>`_, "2,995"
    ac3, `ff-16b-1c-44100hz.ac3 <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.ac3>`_, "2,994"
    aiff, `ff-16b-1c-44100hz.aiff <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.aiff>`_, "33,002"
    amr, `ff-16b-1c-8000hz.amr <https://dl.espressif.com/dl/audio/ff-16b-1c-8000hz.amr>`_, 299
    flac, `ff-16b-1c-44100hz.flac <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.flac>`_, "22,406"
    m4a, `ff-16b-1c-44100hz.m4a <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.m4a>`_, "3,028"
    mp3, `ff-16b-1c-44100hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.mp3>`_, "2,994"
    mp4, `ff-16b-1c-44100hz.mp4 <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.mp4>`_, "3,079"
    ogg, `ff-16b-1c-44100hz.ogg <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.ogg>`_, "2,612"
    opus, `ff-16b-1c-44100hz.opus <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.opus>`_, "2,598"
    wav, `ff-16b-1c-44100hz.wav <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.wav>`_, "49,504"
    wma, `ff-16b-1c-44100hz.wma <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.wma>`_, "3,227"

Playlist containing all above files: `ff-16b-1c-playlist.m3u <https://dl.espressif.com/dl/audio/ff-16b-1c-playlist.m3u>`_

Sample Rates
============

The files in this section have been prepared by converting a single audio file into different sampling rates defined in MPEG Layer III specification. Both mono and stereo versions of files are provided. The bit depth of files is 16 bits.

.. list properties of mp3 files in Linux:
     mp3info -p "%f, %Q, %L-%.1v, %o, %r, %k\n" *.mp3

.. csv-table::
    :header: , Sample Rate, MPEG III, Channels, Bit Rate, Size
    :widths: 30, 20, 10, 20, 10, 10
    :header-rows: 1

    Audio File, [Hz], ver, , [kbit/s], [kB]
    `ff-16b-1c-8000hz.mp3  <https://dl.espressif.com/dl/audio/ff-16b-1c-8000hz.mp3>`_,   8000, 2.5, mono,  8, 183
    `ff-16b-1c-11025hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-11025hz.mp3>`_, 11025, 2.5, mono, 16, 366
    `ff-16b-1c-12000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-12000hz.mp3>`_, 12000, 2.5, mono, 16, 366
    `ff-16b-1c-16000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-16000hz.mp3>`_, 16000, 2,   mono, 24, 548
    `ff-16b-1c-22050hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-22050hz.mp3>`_, 22050, 2,   mono, 32, 731
    `ff-16b-1c-24000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-24000hz.mp3>`_, 24000, 2,   mono, 32, 731
    `ff-16b-1c-32000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-32000hz.mp3>`_, 32000, 1,   mono, 48, "1,097"
    `ff-16b-1c-44100hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.mp3>`_, 44100, 1,   mono, 64, "1,462"
    `ff-16b-2c-8000hz.mp3  <https://dl.espressif.com/dl/audio/ff-16b-2c-8000hz.mp3>`_,   8000, 2.5, joint stereo, 24, 549
    `ff-16b-2c-11025hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-11025hz.mp3>`_, 11025, 2.5, joint stereo, 32, 731
    `ff-16b-2c-12000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-12000hz.mp3>`_, 12000, 2.5, joint stereo, 32, 731
    `ff-16b-2c-16000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-16000hz.mp3>`_, 16000, 2,   joint stereo, 48, "1,097"
    `ff-16b-2c-22050hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-22050hz.mp3>`_, 22050, 2,   joint stereo, 64, "1,462"
    `ff-16b-2c-24000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-24000hz.mp3>`_, 24000, 2,   joint stereo, 64, "1,462"
    `ff-16b-2c-32000hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-32000hz.mp3>`_, 32000, 1,   joint stereo, 96, "2,194"
    `ff-16b-2c-44100hz.mp3 <https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3>`_, 44100, 1,   joint stereo, 128, "2,924"

Playlist containing all above files: `ff-16b-mp3-playlist.m3u <https://dl.espressif.com/dl/audio/ff-16b-mp3-playlist.m3u>`_

Original music file: "Furious Freak", Kevin MacLeod (incompetech.com), Licensed under Creative Commons: By Attribution 3.0, http://creativecommons.org/licenses/by/3.0/
