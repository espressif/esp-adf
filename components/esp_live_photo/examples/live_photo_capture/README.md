# Live Photo Capture Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example captures a short video clip on an Espressif SoC, automatically selects a cover frame, muxes a Motion Photo–style file (JPEG cover + embedded MP4 with audio), writes it to the SD card, and verifies the output by parsing the file.

### Typical Scenarios

- Smart cameras and doorbells
- AI snapshot and IoT imaging devices
- Embedded media recorders that need a still cover plus a short clip in one file

### Technical Details

It demonstrates `esp_capture` for synchronized video and audio capture, a custom video source stage for cover-frame scoring, `esp_live_photo_muxer` for Motion Photo packaging, and `esp_live_photo_extractor` for on-device verification. Board peripherals are brought up through `esp_board_manager`.

### Run Flow
Capture flow:

- Capture → score and pick best frame → encode cover JPEG → mux JPEG + MP4 (video + audio) → write `/sdcard/live_photo.jpg`

Verify flow:
- Open `/sdcard/live_photo.jpg` → open with live photo extractor and print statistics.

## Environment Setup

### Hardware Required

- Espressif development board supported by `esp_board_manager` with:
  - Camera
  - Audio ADC / microphone path
  - microSD card mounted as FatFs volume

We recommend using the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html).

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- Managed components are fetched automatically from `idf_component.yml` when you build (network access on first build).

## Build and Flash

### Build Preparation

Before building, ensure the ESP-IDF environment is set up. If it is not, run the following in the ESP-IDF root directory (full steps in the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html)):

```
./install.sh
. ./export.sh
```

Go to this example directory:

```
cd <path-to-esp-adf>/components/esp_live_photo/examples/live_photo_capture
```

Select the target chip and board configuration with board manager (list boards, then pick one that matches your hardware):

```bash
idf.py gen-bmgr-config -l
idf.py gen-bmgr-config -b esp32_p4_function_ev
```

Replace `esp32_p4_function_ev` with your actual target board id.

### Project Configuration

Change capture parameters in `main/settings.h` instead of menuconfig where possible:

- Video: `VIDEO_WIDTH`, `VIDEO_HEIGHT`, `VIDEO_FPS`, `VIDEO_FORMAT`
- Audio: `AUDIO_FORMAT`, `AUDIO_CHANNEL`, `AUDIO_SAMPLE_RATE`
- Clip length: `LIVE_PHOTO_DURATION_MS`
- JPEG buffer cap: `MAX_JPEG_SIZE`

### Build and Flash Commands

- Build and Flash:

```
idf.py -p PORT flash monitor
```

- Exit the monitor: `Ctrl-]`

## How to Use the Example

### Functionality and Usage

- On boot, the app initializes camera, audio ADC, and SD card via board manager, then runs capture, muxing to `/sdcard/live_photo.jpg`, and verification.
- Cover selection uses a simple scoring strategy (e.g. prefer mid-sequence frames; optional brightness / motion / sharpness–style signals when camera stats are available). You can refine the logic in `live_photo_capture.c`.
- Success is indicated by the log line `Live photo capture and verify success`.

## View Live Photo

Copy `live_photo.jpg` from the SD card to your computer, then open it in an app that supports Motion Photo / embedded MP4 in JPEG (for example the **Windows 11** Photos app), or drop the file into the [Motion Photo Viewer](https://dj0001.github.io/Motion-Photo-Viewer/) page to preview it in the browser.

## Troubleshooting

### SD card or device initialization failed

If logs show `Skip muxer for mount sdcard failed`, check that the card is formatted (FAT), seated correctly, and that the board profile enables the SD card device. Confirm camera and audio ADC init succeed before muxing.

### Capture or muxer errors

Errors such as `Failed to create video source`, `Fail to create capture`, or `Failed to create muxer` usually mean the board pack does not match the hardware, or video/audio pipeline configuration in `settings.h` is not supported by the board encoder paths. Verify `VIDEO_FORMAT` / `AUDIO_FORMAT` against `esp_capture` defaults for your chip.

### Verification failed

`Live photo verify failed` indicates the extractor could not parse the file or read tracks. Ensure the mux step completed and `/sdcard/live_photo.jpg` is not truncated (e.g. full disk or power loss during write).

## Technical Support

- Forum: [esp32.com](https://esp32.com/viewforum.php?f=20)
- ESP-ADF issues: [GitHub esp-adf issues](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
