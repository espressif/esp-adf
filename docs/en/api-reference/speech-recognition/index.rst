******************
Speech Recognition
******************

The ESP-ADF comes complete with :doc:`wakeup word libraries <wakeup-word-libs>` and :doc:`speech recognition interface <esp_sr_iface>` to recognize voice wakeup commands. Most of currently implemented wakeup commands are in Chinese with one command "Alexa" in English. 

Provided in this section functions also include automatic speech detection, also known as :doc:`voice activity detection (VAD) <esp_vad>`, and :doc:`speech recording engine <recorder_engine>`.

The Speech Recognition API is designed to easy integrate with existing :doc:`../framework/index` to retrieve the audio stream from a microphone connected to the audio chip. 

.. toctree::
    :caption: In This Section
    :maxdepth: 1

    wakeup-word-libs
    esp_sr_iface
    esp_vad
    recorder_engine
