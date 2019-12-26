# Example of MicroPython with audio module

(See the README.md file in the upper level 'examples' directory for more information about examples.)

The goal of these examples is to demonstrates how to use the audio module to play audio source. Contains the following scripts:

1. `boot.py`: Network configuration and `sys.path` update.
2. `install_libs.py`: Install libs needed by the examples.
3. `audio_test.py`: Test the audio.player and audio.recorder.
4. `baidu_tts.py`: Connect to baidu tts service, get result with a given string, save it to file and play it.
5. `aws_polly.py`: Connect to aws polly service, get result with a given string, save it to file and play it.

## How to use example

### Hardware Required

These examples is based on audio boards:

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:---------------:|:----------------:|
| [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../docs/_static/yes-button.png "Compatible") | ![alt text](../../docs/_static/yes-button.png "Compatible") |![alt text](../../docs/_static/yes-button.png "Compatible") |

SDCard is needed to store the scripts and libs.

### Configure the project

Refer to `README.md` file in the upper level directory for more information about configuration.

Make sure the right audio board is selected in `sdkconfig.adf`.

### Build and Flash

Refer to `README.md` file in the upper level directory for more information about build and flash.

### Usage

Please try this examples with the following steps:

1. Modify the network setting in 'boot.py' to yours.
2. Copy all the python files under this directory to the root of SDCard.
3. Power on the board, 'boot.py' will be run automatic to set the network and system path.
4. Connect the board with a USB cable and open the UART port with a serial tool such as `minicom` on Ubuntu.
5. Install the libs with the following steps:

    ```python
    >>> import install_libs
    >>> install_libs.install()
    ```

6. Run other examples:

    ```python
    >>> import audio_test
    >>> audio_test.run()
    ```

    ```python
    >>> import baidu_tts
    >>> baidu_tts.run('your baidu access', 'your baidu secret')
    ```

    ```python
    >>> import aws_polly
    >>> aws_polly.run(b'us-east-1', b'your aws access', b'your aws secret')
    ```

## Example Output

Power on with correct network setting:

```python
connecting to network...
network config: ('192.168.50.97', '255.255.255.0', '192.168.50.1', '192.168.50.1')
['', '/lib', '/sdcard/lib', '/sdcard']
MicroPython v1.11-497-gf301170c7-dirty on 2019-12-17; ESP32 module (spiram) with ESP32
Type "help()" for more information.
>>>
```

Install libs as follows:

```python
>>> import install_libs
>>> install_libs.install()
Installing to: /sdcard/lib/
Warning: micropython.org SSL certificate is not validated
Installing micropython-json 0.1 from https://micropython.org/pi/json/json-0.1.tar.gz
Installing micropython-re-pcre 0.2.5 from https://micropython.org/pi/re-pcre/re-pcre-0.2.5.tar.gz
Installing micropython-ffilib 0.1.3 from https://micropython.org/pi/ffilib/ffilib-0.1.3.tar.gz
Installing to: /sdcard/lib/
Installing micropython-urequests 0.6 from https://micropython.org/pi/urequests/urequests-0.6.tar.gz
Installing to: /sdcard/lib/
Installing micropython-hmac 3.4.2.post3 from https://micropython.org/pi/hmac/hmac-3.4.2.post3.tar.gz
Installing micropython-hashlib 2.4.0.post4 from https://micropython.org/pi/hashlib/hashlib-2.4.0.post4.tar.gz
Installing micropython-warnings 0.1.1 from https://micropython.org/pi/warnings/warnings-0.1.1.tar.gz
>>>
```

Run `audio_test.py`:

```python
>>> import audio_test
>>> audio_test.run()
======================== Player ========================

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                                ESP_AUDIO-v*.*                              |
|                     Compile date: MM DD YYYY-HH:mm:SS                     |
------------------------------------------------------------------------------
Set volume to:  35
Play source:  https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3
Playing: *******************************************************

===================== Record AMR ====================
Recording voice to a AMR file in 10 seconds. Please say something via microphone.
Recording: **********
Playback the recording file
Playing: **********

============== Record AMR with maxtime ==============
Recording voice to a AMR file in 10 seconds. Please say something via microphone.
Recording: **********
Playback the recording file
Playing: *********

========= Record WAV with maxtime and endcb =========
Recording voice to a WAV file in 10 seconds. Please say something via microphone.
Recording: *********
Recoder state:  False
Playback the recording file
Playing: **********

>>>
```

Run `baidu_tts.py`:

```python
>>> import baidu_tts
>>> baidu_tts.run('your baidu access', 'your baidu secret')
Request the access token
Get Response
Get content and save to /sdcard/tts.mp3
Create player
Play source:  file://sdcard/tts.mp3
Playing: *******

End
>>>
```

Run `aws_polly.py`

```python
>>> import aws_polly
>>> aws_polly.run(b'us-east-1',  b'your aws access', b'your aws secret')
current time is:  (2019, 12, 18, 9, 11, 38, 2, 352)
https://polly.us-east-1.amazonaws.com/v1/speech
status code:  200
 reason code:  b'OK'
 Content len:  143927
Play source:  file://sdcard/polly.mp3
Playing: ************************

End
>>>
```

## Troubleshooting

### ImportError: no module named '****'

1. Make sure `install_libs.install()` had been called, this will install all the modules needed by these examples.
2. Make sure all the libs are installed correctly, please refer to the output of `install_libs.py` above.


### `aws_polly.py` IndexError: list index out of range

```python
>>> aws_polly.run(b'us-east-1', b'your aws access', b'your aws secret')
current time is:  (2019, 12, 17, 9, 46, 2, 1, 351)
https://polly.us-east-1.amazonaws.com/v1/speech
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
  File "/sdcard/aws_polly.py", line 84, in run
  File "/sdcard/lib/urequests.py", line 115, in post
  File "/sdcard/lib/urequests.py", line 54, in request
IndexError: list index out of range
>>>
```

No correct response when `usocket.getaddrinfo(host, port, 0, usocket.SOCK_STREAM)`.

```python
>>> aws_polly.run(b'us-east-1', b'your aws access', b'your aws secret')
current time is:  (2019, 12, 17, 12, 50, 34, 1, 351)
https://polly.us-east-1.amazonaws.com/v1/speech
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
  File "/sdcard/aws_polly.py", line 84, in run
  File "/sdcard/lib/urequests.py", line 115, in post
  File "/sdcard/lib/urequests.py", line 84, in request
IndexError: list index out of range
>>>
```

No http status get from server.

All these errors, please try again, or switch to anther network and try.

### Micropython ENV issue

1. “python2: No such file or directory”
run `sudo ln -sf /usr/bin/python2.7 /usr/local/bin/python2 `
more details, https://github.com/clinton-hall/nzbToMedia/issues/454

2. “No module named 'pyparsing' ”
run `python3 -m pip install pyparsing==2.3.1`
more details, https://github.com/micropython/micropython/issues/5269
