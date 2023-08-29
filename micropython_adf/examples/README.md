# Example of MicroPython with audio module

(See the README.md file in the upper level 'examples' directory for more information about examples.)

The goal of these examples is to demonstrates how to use the audio module to play audio source. Contains the following scripts:

1. `connect.py`: Network connect interface.
2. `install_libs.py`: Install libs needed by the examples.
3. `audio_test.py`: Test the audio.player and audio.recorder.
4. `baidu_tts.py`: Connect to baidu tts service, get result with a given string, save it to file and play it.
5. `aws_polly.py`: Connect to aws polly service, get result with a given string, save it to file and play it.

## How to use example

### Hardware Required

Supported audio boards:

| ESP32-LyraT | ESP32-LyraT-Mini | ESP32-S3-Korvo2-V3 |
|:-----------:|:---------------:|:---------------:|
| [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | [![alt text](../../docs/_static/esp32-s3-korvo-2-v3.0-small.png "ESP32-S3-Korvo2-v3")](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) |
| ![alt text](../../docs/_static/yes-button.png "Compatible") | ![alt text](../../docs/_static/yes-button.png "Compatible") | ![alt text](../../docs/_static/yes-button.png "Compatible") |

SDCard is needed to store the scripts and libs.

### Configure the project

Refer to `README.md` file in the upper level directory for more information about configuration.

### Build and Flash

Refer to `README.md` file in the upper level directory for more information about build and flash.

### Usage

Please try this examples with the following steps:

1. Copy all the python files under this directory to the root of SDCard.
2. Power on the board.
3. Connect the board with a USB cable and open the UART port with a serial tool such as `minicom` on Ubuntu.
4. Connect to the network:

    ```python
    >>> import connect
    >>> connect.do_connect('ssid', 'password')
    ```

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

Power on with SDCard inserted:

```python
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:4672
load:0x40078000,len:14548
ho 0 tail 12 room 4
load:0x40080400,len:3364
0x40080400: _init at ??:?

entry 0x400805cc
['', '.frozen', '/lib', '/sdcard', '/sdcard/lib']
MicroPython v1.20.0-344-g62d35fa35 on 2023-08-10; ESP32 module with ESP32
Type "help()" for more information.
>>>
```

Connect to network:

```python
>>> import connect
>>> connect.do_connect('ssid', 'password')
connecting to network...
network config: ('192.168.1.219', '255.255.255.0', '192.168.1.1', '192.168.1.1')
>>>
```

Install libs as follows:

```python
>>> import install_libs
>>> install_libs.install()
Installing json (latest) from https://micropython.org/pi/v2 to /sdcard/lib
Copying: /sdcard/lib/json/__init__.mpy
Copying: /sdcard/lib/json/encoder.mpy
Copying: /sdcard/lib/json/scanner.mpy
Copying: /sdcard/lib/json/tool.mpy
Copying: /sdcard/lib/json/decoder.mpy
Done
Installing urequests (latest) from https://micropython.org/pi/v2 to /sdcard/lib
Copying: /sdcard/lib/requests/__init__.mpy
Copying: /sdcard/lib/urequests.mpy
Done
Installing hmac (latest) from https://micropython.org/pi/v2 to /sdcard/lib
Copying: /sdcard/lib/hmac.mpy
Done
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
Set volume to:  40
Play source:  https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3
{'media_src': 0, 'status': 1, 'err_msg': 0}
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
