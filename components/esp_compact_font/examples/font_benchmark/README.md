# Font Benchmark

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - compares GFH and LVGL font binaries, then draws both on a board-manager LCD

## Example Brief

- This example opens one CJK corpus as an LVGL `.bin` and as a GFH atlas, times a full decode pass with the glyph cache off, and prints `BENCH` / `COMPARE` lines. It then draws paired rows on the panel at several pixel sizes and walks a karaoke-style highlight across those rows.
- It shows `esp_compact_font_open`, a memory font source, `esp_compact_font_decode_at`, runtime `esp_compact_font_set_pixel`, binding one glyph cache, and `esp_compact_font_set_draw_range` for the highlight.

### Resources

#### Host reference

Reference for the corpus from `prepare_fonts.py` (9620 glyphs, 36 px). Built with `-O2`, cache off. `./host_bench.sh` checks these sizes and that every glyph decodes.

| bpp | LVGL KB | GFH KB | LVGL µs/glyph | GFH µs/glyph |
| --- | ------: | -----: | ------------: | -----------: |
| 1   |    1335 |    788 |          2.71 |         1.97 |
| 2   |    1778 |   1474 |          9.14 |         2.88 |
| 4   |    3066 |   2970 |          9.90 |         3.66 |

#### ESP32-S3 on-board reference

Measured on an ESP32-S3 at 240 MHz with 80 MHz PSRAM. The font source is copied into RAM (`with_ram=1`) before decoding; the glyph cache is off. Every row decoded all 9620 glyphs at 36 px.

| bpp | LVGL open ms | GFH open ms | LVGL decode ms | GFH decode ms | LVGL µs/glyph | GFH µs/glyph |
| --- | -----------: | ----------: | -------------: | ------------: | -------------: | ------------: |
| 1   |      164.131 |      88.408 |       1757.046 |      1242.523 |         182.64 |        129.16 |
| 2   |      194.160 |     135.454 |       4203.172 |      1808.119 |         436.92 |        187.95 |
| 4   |      279.157 |     233.920 |       4392.224 |      2158.726 |         456.57 |        224.39 |

`open ms` includes preparing the copied RAM source. `decode ms` is the complete uncached pass; `µs/glyph` is that time divided by 9620. Timing depends on the board, memory configuration, compiler, and ESP-IDF version.

### Prerequisites

- ESP-IDF release/v5.4 or later, with `IDF_PATH` set
- Familiarity with [`esp_compact_font`](../../README.md)
- A board definition supported by `esp_board_manager` that provides `display_lcd`

### Folder Contents

```text
font_benchmark/
├── main/
│   └── app_main.c              Decode bench and LCD paint
├── fonts/                      Atlases from prepare_fonts.py
├── prepare_fonts.py            Download, subset, and convert the corpus
├── host_bench.c                Host decode twin
├── host_bench.sh               Build the host twin and check the corpus
├── host_codec.ld               Keeps the host codec section
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## Environment Setup

### Hardware Required

- An ESP development board with board-manager support for `display_lcd`

### Additional Requirements

Generate the atlases before configuring the project. The build does not download fonts.

```bash
python3 prepare_fonts.py
```

Outputs land in `fonts/`: `lvgl_{1,2,4}bpp.bin` and `gfh_{1,2,4}bpp.gfh`. The build embeds those files.

Check the same files on the PC (no LCD):

```bash
./host_bench.sh
```

The script compiles `host_bench.c` with the component sources, runs it on `fonts/`, and fails if an atlas is missing, a glyph fails to decode, or the file sizes differ from the table above.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.4 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

`sdkconfig.defaults` selects performance optimization, SPIRAM malloc, the custom partition table, and LVGL `.bin` support.

Generate board-manager configuration before building:

```bash
idf.py gen-bmgr-config -b <your_board_name>
```

LVGL `.bin` support is required for this example. It is already set in `sdkconfig.defaults`:

```text
ESP Compact Font > LVGL bin font
```

### Build and Flash

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Example Functionality

After boot the example decodes each bpp twice (font bytes copied into RAM, then left in flash) and logs one `BENCH` line per codec plus one `COMPARE` line per pair. It then opens the LCD through `esp_board_manager` and cycles pixel sizes 22, 36, 48, and 64. Each size draws an LVGL row and a GFH row, then highlights both together.

`with_ram=1` is the RAM copy. `with_ram=0` reads the embedded atlas in place. `bad` counts glyphs that failed to decode.

### References

- Component README: [esp_compact_font](../../README.md)

## Troubleshooting

- **`BENCH bit=N codec=lvgl missing` or `codec=gfh missing`**: run `python3 prepare_fonts.py` and build again so the atlases are embedded.
- **`./host_bench.sh` reports `atlas not ready`**: the files under `fonts/` are missing or too small. Run `prepare_fonts.py` in this directory.
- **LCD init failed / no `display_lcd` handle**: run `idf.py gen-bmgr-config -b <your_board_name>` for a board that provides `display_lcd`, then rebuild.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
