Sonic
=====

The Sonic component acts as a multidimensional filter that lets you adjust audio parameters of a WAV stream. This functionality may be useful to e.g. increase playback speed of an audio recording by a user selectable rate.

The following parameters can be adjusted:

    * speed
    * pitch
    * interpolation type
    
The adjustments of the first two parameters are represented by `float` values that provide the rate of adjustment. For example, to increase the speed of an audio sample by 2 times, call ``sonic_set_pitch_and_speed_info(el, 1.0, 2.0)``. To keep the speed as it is, call ``sonic_set_pitch_and_speed_info(el, 1.0, 1.0)``.

For the `interpolation type` you may select either faster but less accurate linear interpolation, or slower but more accurate FIR interpolation.


Application Example
-------------------

Implementation of this API is demonstrated in :example:`audio_processing/pipeline_sonic` example.


API Reference
-------------

.. include:: /_build/inc/audio_sonic.inc
