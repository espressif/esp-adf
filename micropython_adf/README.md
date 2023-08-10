# Integrate the ADF components as parts of MicroPython

## Prepare

To use this integration, please read the [documents](https://github.com/micropython/micropython) of `MicroPython` and the [README](https://github.com/micropython/micropython/tree/master/ports/esp32) of `MicroPython port for ESP32` first.

## Build and flash

To build the project, please follow the steps below:

* Clone `ESP-ADF`, `ESP-IDF`, `MicroPython` into your workspace and update all the submodules.
* `export ADF_PATH={ your ESP-ADF's path }`.
* `export IDF_PATH={ your ESP-IDF's path }`.
* `ESP-IDF`:

  > 1. Checkout v5.0.2: `git checkout tags/v5.0.2`.
  > 2. Install espressif tools: `./install.sh`.
  > 3. Apply the patch: `git apply ${ADF_PATH}/idf_patches/idf_v5.0_freertos.patch`
  > 4. Remove the `fatfs` component due to the complie issue: `rm ${IDF_PATH}/components/fatfs/CMakeLists.txt`.
  > 5. `. $IDF_PATH/export.sh`

* `MicroPython`:

  > 1. Checkout the supported commit: `git checkout d529c2067 -b { branch name you wanted }`.
  > 2. Apply the patch: `git apply ${ADF_PATH}/micropyton_adf/mp.diff`.
  > 3. `cd ${YOUR_MP_PATH}/ports/esp32`
  > 4. Select the borad with `-D MICROPY_BOARD_DIR=`, build and flash: `idf.py build -D MICROPY_BOARD_DIR=${ADF_PATH}/micropython_adf/boards/lyrat43 -D USER_C_MODULES=${ADF_PATH}/micropython_adf/mod/micropython.cmake flash monitor`.

## Libraries

### `audio` - player and recorder

This module implements player and recorder

- player: encapsulates the ADF esp_audio, support local and online resources.
- recorder: record and encoding the voice into file.

NOTICE:
> This a beta release as version '0.5-beta2',
> Classes and methods may be changed by further release.

#### Constructors

- `class audio.player([state_callback])`

  Create a player, `state_callback` is a monitor of player state, when state changed, this callback will be invoked.

  ```python
  def cb(state):
      print(state) #'{{'media_src': 0, 'err_msg': 0, 'status': 1}}', media_src is reserved.

  p = audio.player(cb)
  # play music
  ```

- `class audio.recorder()`

  Create recorder.

  ```python
  r = audio.recorder()
  ```

#### Functions

- `audio.verno()`

  version of audio module

  ```python
  >>> audio.verno()
  '0.5-beta1'
  ```

- `audio.mem_info()`

  Show memory usage.

  ```Python
  >>> audio.mem_info()
  {'dram': 176596, 'inter': 212472, 'mem_total': 1197704}
  ```

#### Constants

Audio error type:

- `audio.AUDIO_OK`: ESP_ERR_AUDIO_NO_ERROR
- `audio.AUDIO_FAIL` : ESP_ERR_AUDIO_FAIL
- `audio.AUDIO_NO_INPUT_STREAM` : ESP_ERR_AUDIO_NO_INPUT_STREAM
- `audio.AUDIO_NO_OUTPUT_STREAM` : ESP_ERR_AUDIO_NO_OUTPUT_STREAM
- `audio.AUDIO_NO_CODEC` : ESP_ERR_AUDIO_NO_CODEC
- `audio.AUDIO_HAL_FAIL` : ESP_ERR_AUDIO_HAL_FAIL
- `audio.AUDIO_MEMORY_LACK` : ESP_ERR_AUDIO_MEMORY_LACK
- `audio.AUDIO_INVALID_URI` : ESP_ERR_AUDIO_INVALID_URI
- `audio.AUDIO_INVALID_PARAMETER` : ESP_ERR_AUDIO_INVALID_PARAMETER
- `audio.AUDIO_NOT_READY` : ESP_ERR_AUDIO_NOT_READY
- `audio.AUDIO_TIMEOUT` : ESP_ERR_AUDIO_TIMEOUT
- `audio.AUDIO_ALREADY_EXISTS` : ESP_ERR_AUDIO_ALREADY_EXISTS
- `audio.AUDIO_LINK_FAIL` : ESP_ERR_AUDIO_LINK_FAIL
- `audio.AUDIO_OPEN` : ESP_ERR_AUDIO_OPEN
- `audio.AUDIO_INPUT` : ESP_ERR_AUDIO_INPUT
- `audio.AUDIO_PROCESS` : ESP_ERR_AUDIO_PROCESS
- `audio.AUDIO_OUTPUT` : ESP_ERR_AUDIO_OUTPUT
- `audio.AUDIO_CLOSE` : ESP_ERR_AUDIO_CLOSE

### class `audio.player`

Audio player now can support `mp3`,`amr` and `wav`, if more types are needed, please add the decoder in function `audio_player_create`.

#### Methods

- `player.play(uri, pos=0, sync=False)`
  Play the `uri` from specified position. if `sync = True`, the thread will be suspended until the music finished. `uri` formated as "file://sdcard/test.wav" or "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3".

- `player.stop(termination=player.TERMINATION_NOW)`
  Stop the player

- `player.pause()`
  Pause the player

- `player.resume()`
  Resume the player

- `player.set_vol(vol)`
  Set the volume of the audio board.

- `player.get_vol()`
  Get the volume of the audio board.

- `player.get_state()`
  Get the state of the player.

  ```python
  >>> p = audio.player(None)
  >>> p.get_state()
  {'media_src': 0, 'err_msg': 0, 'status': 0}
  ```

- `player.pos()`
  Get the position in bytes of currently played music.

- `player.time()`
  Get the position in microseconds of currently played music.

#### Constants

Player termination type:

- `TERMINATION_NOW`: TERMINATION_TYPE_NOW
- `TERMINATION_DONE`: TERMINATION_TYPE_DONE

Player status type:

- `STATUS_UNKNOWN`: AUDIO_STATUS_UNKNOWN
- `STATUS_RUNNING`: AUDIO_STATUS_RUNNING
- `STATUS_PAUSED`: AUDIO_STATUS_PAUSED
- `STATUS_STOPPED`: AUDIO_STATUS_STOPPED
- `STATUS_FINISHED`: AUDIO_STATUS_FINISHED
- `STATUS_ERROR`: AUDIO_STATUS_ERROR

### class `audio.recorder`

#### Methods

- `recorder.start(path, format=PCM, maxtime=0, endcb=None)`
  Start the recorder and save the voice into `path` with encoding the date as `format`.
  If `maxtime` > 0, recorder will stop automatic when the duration passed, `endcb` will be invoked if not `None`.
  If `maxtime` == 0, recorder will work until `recorder.stop()` is called.

- `recorder.stop()`
  Stop recording.

- `recorder.is_running()`
  Get the state of recorder, `Bool`.

#### Constants

format of final data:

- `recorder.PCM`
- `recorder.AMR`
- `recorder.MP3`
- `recorder.WAV`
