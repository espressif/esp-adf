HTTP Client
===========

A common task of ESP32 is to request contents provided by external severs using HTTP or HTTPS protocol. The **HTTP Client** provides means to implement this functionality. 

The HTTP / HTTPS protocol provides several methods for the client to interact with the server. The methods supported by **HTTP Client** are listed in :cpp:type:`esp_http_client_method_t`.

One of use cases may be recording a speech message by an audio board, sending to the cloud service for speech-to-text recognition using HTTPS POST method, and retrieving back recognized message as a text file.

Application Examples
--------------------

Implementation of this API is demonstrated in the following examples:

* :example:`cloud_services/pipeline_aws_polly_mp3`
* :example:`cloud_services/pipeline_baidu_speech_mp3`


.. include:: /_build/inc/esp_http_client.inc
