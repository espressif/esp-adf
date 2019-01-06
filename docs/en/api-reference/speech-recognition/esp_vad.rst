Voice Activity Detection
========================

Voice activity detection (VAD) is a technique used in speech processing to detect the presence (or absence) of human speech. Detection of somebody speaking may be used to activate some processes, e.g. automatically switch on voice recording. It may be also used to deactivate processes, e.g. stop coding and transmission of silence packets to save on computation and network bandwidth. 

Provided in this section API implements VAD functionality together with couple of options to configure sensitivity of speech detection, set sample rate or duration of audio samples.


Application Example
-------------------

Implementation of the voice activity detection API is demonstrated in :example:`speech_recognition/vad` example.


API Reference
-------------

.. include:: /_build/inc/esp_vad.inc
