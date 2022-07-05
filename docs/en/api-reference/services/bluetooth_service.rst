Bluetooth Service
=================

:link_to_translation:`zh_CN:[中文]`

The Bluetooth service is dedicated to interface with Bluetooth devices and provides support for the following protocols:

* HFP (Hands-Free Profile): remotely controlling the mobile phone by the Hands-Free device and the voice connections between them
* A2DP (Advanced Audio Distribution Profile): implementing streaming of multimedia audio using a Bluetooth connection
* AVRCP (Audio Video Remote Control Profile): used together with A2DP for remote control of devices such as headphones, car audio systems, or speakers


Application Example
-------------------

Implementation of this API is demonstrated in the following example:

* :example:`player/pipeline_bt_sink`


.. include:: /_build/inc/bluetooth_service.inc
.. include:: /_build/inc/bt_keycontrol.inc
.. include:: /_build/inc/hfp_stream.inc
.. include:: /_build/inc/a2dp_stream.inc