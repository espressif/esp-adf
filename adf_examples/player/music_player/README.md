# SD Card Music Player

- [中文版本](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates an SD card music player with a touch-screen UI. It scans local music files on a microSD card, shows the player screen, song information, and playback controls on the display, and plays music through the board audio output device.

### Typical Use Cases

- Local music player with touch screen
- SD card audio scanning and list playback
- On-screen player UI synchronized with playback state

### Runtime Flow

1. `esp_board_manager` initializes SD card, audio DAC, LCD, and touch
2. `esp_media_db` scans `/sdcard` for `.mp3`, `.aac`, and `.wav` files (scan depth is 1)
3. `esp_playlist` imports scan results and manages repeat modes (one / all / shuffle)
4. `esp_audio_simple_player` decodes asynchronously; PCM is written directly to the codec with `esp_codec_dev_write()`
5. `esp_lvgl_adapter` runs the LVGL task, shows `lv_demo_music`, and overlays song title plus touch controls

### File Structure

```
music_player/
├── assets/                 Place font.ttf here to embed it into the assets partition
├── main/
│   ├── app_main.c              Application entry
│   ├── music_player_config.h   Default volume, font path, and related macros
│   ├── music_player_board.c/h   Board peripheral init
│   ├── music_player_display.c/h LVGL adapter and display registration
│   ├── music_player_playback.c/h Playlist and simple player control
│   ├── music_player_ui.c/h       LVGL music demo and touch controls
│   └── idf_component.yml
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── pytest_music_player.py
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Requirements

- microSD card (FAT filesystem)
- Speaker or headphones connected to the board audio output
- LCD and touch input devices

### Supported IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- Copy `.mp3`, `.aac`, or `.wav` test files into the `/sdcard` mount point (UTF-8 filenames supported)
- To use FreeType for Chinese rendering, place `font.ttf` under `assets/` before build; it is embedded into the Flash assets partition (ensure redistribution is allowed). Without it, the built-in CJK font is used

## Build and Flash

### Build Preparation

Make sure ESP-IDF is configured before building this example. If it is already set up, enter the project directory directly. Otherwise, run the following scripts in the ESP-IDF root directory. See the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/) for the target chip you use.

```
./install.sh
. ./export.sh
```

Quick steps:

- Enter the example directory:

```
cd adf_examples/player/music_player
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager). Install [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) to select the development board with the commands below.

- Install the helper in the activated ESP-IDF Python environment (once per environment):

```bash
pip install esp-bmgr-assist
```

- Select a board. The selected board must provide `fs_sdcard`, `audio_dac`, `display_lcd`, and `lcd_touch` devices:

```bash
idf.py bmgr -b <board_name>

# Example board used for this README validation
idf.py bmgr -b esp32_p4_function_ev
```

> [!NOTE]
> To switch to another supported board, run the same command with a different board name.
> For custom boards, see the [custom board guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/docs/how_to_customize_board_en.md).
> For more information, see the [ESP Board Manager getting started guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

### Project Configuration

Default behavior is defined in `main/music_player_config.h` and `sdkconfig.defaults`; extra menuconfig is usually unnecessary. Common tunables:

- `MUSIC_PLAYER_DEFAULT_VOLUME`: default playback volume (currently 70)
- `MUSIC_PLAYER_FONT_PATH` / `MUSIC_PLAYER_FONT_SIZE`: FreeType font path and size (default `F:font.ttf`, 24)
- `MUSIC_PLAYER_SCAN_DEPTH`: media DB scan depth (currently 1, scanning `/sdcard`)

For FreeType, built-in CJK font, and other LVGL options, use `idf.py menuconfig` under `Component config` → `ESP LVGL Adapter` and `LVGL configuration`.

### Build and Flash

- Build the example

```
idf.py build
```

- Flash and monitor (replace PORT with your serial port):

```
idf.py -p PORT flash monitor
```

- Press `Ctrl-]` to exit the monitor

## How to Use the Example

### Features and Usage

1. Copy music files into the `/sdcard` mount point or one of its first-level subdirectories (see `scan dir: /sdcard` in the log below)
2. After boot, the example scans the folder; if tracks are found it plays the first one, otherwise the UI shows no music available
3. Use the bottom touch control bar:
   - Previous / play or pause / next
   - Loop mode button cycles one-track repeat, list repeat, and shuffle
4. The top overlay shows the current song title (ellipsis mode, Chinese supported) and repeat mode
5. The background uses the LVGL built-in `lv_demo_music` UI; the progress bar is demo animation only
6. When the current track finishes, the next track starts automatically according to the selected mode

### Log Output

The following is a continuous on-target log from ESP32-P4 Function EV, including step logs `[ 1 ]` through `[ 5 ]`, `Scanned N tracks`, and `Play file:` lines. LCD, touch, and codec initialization logs vary with the selected board configuration.

```text
I (1229) main_task: Calling app_main()
I (1229) MUSIC_PLAYER: [ 1 ] Initialize board peripherals
I (1229) PERIPH_LDO: LDO initialize success
W (1229) ldo: The voltage value 0 is out of the recommended range [500, 2700]
I (1229) DEV_FS_FAT_SUB_SDMMC: slot_config: cd=-1, wp=-1, clk=0, cmd=0, d0=0, d1=0, d2=0, d3=0, d4=0, d5=0, d6=0, d7=0, width=4, flags=0x1
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1276) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (1276) BOARD_MANAGER: Device fs_sdcard initialized
I (1277) BOARD_DEVICE: Device handle fs_sdcard found, Handle: 0x4ffbb43c TO: 0x4ffbb43c
I (1278) PERIPH_I2S: I2S[0] STD,  TX, ws: 10, bclk: 12, dout: 9, din: 11
I (1278) PERIPH_I2S: I2S[0] initialize success: 0x4800822c
I (1278) DEV_AUDIO_CODEC: DAC is ENABLED
I (1278) PERIPH_GPIO: Initialize success, pin: 53, set the default level: 0
I (1279) PERIPH_I2C: I2C master bus initialized successfully
I (1284) ES8311: Work in Slave mode
I (1287) DEV_AUDIO_CODEC: Successfully initialized codec: audio_dac
I (1287) DEV_AUDIO_CODEC: Create esp_codec_dev success, dev:0x4ffbc284, chip:es8311
I (1287) BOARD_MANAGER: Device audio_dac initialized
I (1287) BOARD_DEVICE: Device handle audio_dac found, Handle: 0x4ffbb854 TO: 0x4ffbb854
I (1288) I2S_IF: No paired data, current mode: playback
I (1288) I2S_IF: STD: TX, data_bit: 16, slot_bit: 16, ws_width: 16, slot_mode: STEREO, slot_mask: 0x3
I (1289) I2S_IF: STD: TX, sample_rate_hz: 48000, mclk_multiple: 256, clk_src: 0
I (1303) Adev_Codec: Open codec device OK
I (1303) MUSIC_PLAYER_BOARD: SD card mounted at /sdcard, scan dir: /sdcard
I (1304) MUSIC_PLAYER: [ 2 ] Initialize display and LVGL music UI
I (1304) DEV_DISPLAY_LCD: Initializing LCD display: display_lcd, chip: ek79007, sub_type: dsi
I (1304) DEV_DISPLAY_LCD_SUB_DSI: Initializing DSI LCD display: display_lcd, chip: ek79007
I (1304) BOARD_PERIPH: Reuse periph: ldo_mipi, ref_count=2
I (1306) PERIPH_DSI: MIPI DSI bus initialize success
I (1306) ek79007: version: 1.0.4
E (1467) lcd_panel: esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel
W (1467) DEV_DISPLAY_LCD: Failed to swap LCD panel XY: ESP_ERR_NOT_SUPPORTED
E (1467) lcd_panel: esp_lcd_panel_disp_on_off(71): disp_on_off is not supported by this panel
I (1467) DEV_DISPLAY_LCD: Successfully initialized LCD display: display_lcd (sub_type: dsi), panel: 0x4ffbd65c, io: 0x4ffbd610
I (1468) BOARD_MANAGER: Device display_lcd initialized
I (1468) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=2
I (1468) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (1470) GT911: TouchPad_ID:0x39,0x31,0x31
I (1470) GT911: TouchPad_Config_Version:89
I (1470) DEV_LCD_TOUCH_SUB_I2C: Successfully initialized LCD touch: lcd_touch, addr: 0xba, touch:0x48134cd4, io:0x4ffbdab8
I (1470) DEV_LCD_TOUCH: Successfully initialized LCD touch: lcd_touch, chip: gt911, sub_type: i2c
I (1471) BOARD_MANAGER: Device lcd_touch initialized
I (1471) BOARD_DEVICE: Device handle display_lcd found, Handle: 0x4ffbbe14 TO: 0x4ffbbe14
I (1471) BOARD_DEVICE: Device display_lcd config found: 0x40232fe0 (size: 124)
I (1471) BOARD_DEVICE: Device handle lcd_touch found, Handle: 0x4ffbda30 TO: 0x4ffbda30
I (1472) esp_lvgl:adapter: LVGL adapter initialized successfully
I (1474) esp_lvgl:bridge_v9: Initializing hardware resources
I (1474) esp_lvgl:bridge_v9: Hardware resources initialized successfully
W (1475) esp_lvgl:touch: LV_USE_GESTURE_RECOGNITION is disabled; only single-point pointer events are available
I (1475) esp_lvgl:touch: Touch input device registered successfully (IRQ mode: disabled)
I (1476) mmap_assets: MMAP Assets [assets] mounted successfully. (Lib: v2.0.0, Bin: v1.0.0)
I (1476) lv_fs: Drive 'F' successfully created, version: 1.0.1
I (1476) esp_lvgl:adapter: File system mounted successfully
I (1476) MUSIC_PLAYER_DISPLAY: LVGL assets FS mounted as F:
I (1476) MUSIC_PLAYER_DISPLAY: Display initialized: 1024x600 (dsi)
I (1532) MUSIC_PLAYER_UI: Hidden lv_demo_music title box
I (1536) MUSIC_PLAYER_UI: Use FreeType font: F:font.ttf
I (1540) esp_lvgl:adapter: LVGL task started successfully
I (1540) MUSIC_PLAYER: [ 3 ] Scan SD card playlist from /sdcard
I (1878) MUSIC_PLAYER_PLAYBACK: Scanned 365 tracks under /sdcard
I (1890) MUSIC_PLAYER: [ 4 ] Start playback controller
I (1891) ASP_POOL: Dest rate:48000
I (1891) ASP_POOL: Dest channels:2
I (1891) ASP_POOL: Dest bits:16
I (1892) MUSIC_PLAYER: [ 5 ] Music player ready
I (1892) MUSIC_PLAYER_PLAYBACK: Play file: /sdcard/test_1.mp3
E (1892) ESP_GMF_PIPELINE: esp_gmf_pipeline.c:536 (esp_gmf_pipeline_stop): Got NULL Pointer
I (1892) AUD_SIMP_PLAYER: Use the tag of io from pool, in_str:io_file
I (1893) ESP_GMF_FILE: Open, dir:1, uri:/sdcard/test_1.mp3
I (1896) ESP_GMF_FILE: File size: 4784128 byte, file position: 0
I (1897) ESP_ES_PARSER: The version of es_parser is v1.0.1
W (1898) ESP_GMF_ASMP_DEC: Not enough memory for out, need:4608, old: 1024, new: 4608
I (7892) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
I (7893) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
I (7893) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7894) : │ lvgl              │ 0        │ 408646      │  20.43% │ 6        │ 4056      │ Suspended  │ Extr  │
I (7895) : │ IDLE0             │ 0        │ 304097      │  15.20% │ 0        │ 1228      │ Ready      │ Intr  │
I (7895) : │ TSK_0x4ff5bd14    │ 0        │ 271071      │  13.55% │ 10       │ 3840      │ Blocked    │ Intr  │
I (7896) : │ esp_timer         │ 0        │ 15753       │   0.79% │ 22       │ 3764      │ Suspended  │ Intr  │
I (7896) : │ main              │ 0        │ 460         │   0.02% │ 1        │ 1428      │ Running    │ Intr  │
I (7897) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 708       │ Suspended  │ Intr  │
I (7897) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7898) : │ IDLE1             │ 1        │ 482368      │  24.12% │ 0        │ 1220      │ Ready      │ Intr  │
I (7898) : │ ipc1              │ 1        │ 0           │   0.00% │ 24       │ 716       │ Suspended  │ Intr  │
I (7908) : │ music_ctrl        │ 1        │ 0           │   0.00% │ 5        │ 3640      │ Blocked    │ Intr  │
I (7909) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7910) : │ swdraw            │ 7fffffff │ 518961      │  25.95% │ 3        │ 14348     │ Ready      │ Intr  │
I (7910) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1724      │ Blocked    │ Intr  │
I (7911) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘
```

## Troubleshooting

### Music Files Not Found

If scan fails or you see `No music found`, verify that the microSD card is mounted and `/sdcard` contains `.mp3`, `.aac`, or `.wav` files.

### Playback Failure

If you see `Failed to run player` or `Failed to open on read`, check file integrity, supported format, and SD card connection.

### Chinese Text Rendering Issues

If the assets partition does not contain `font.ttf`, the example falls back to the built-in CJK font. For better Chinese rendering, place `font.ttf` in `assets/` and rebuild before flashing.

## References

- [ESP Audio Simple Player](https://components.espressif.com/components/espressif/esp_audio_simple_player)
- [ESP Board Manager](https://github.com/espressif/esp-board-manager)
- [ESP LVGL Adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter)

## Technical Support

- Forum: [esp32.com](https://esp32.com/viewforum.php?f=20)
- Issues: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
