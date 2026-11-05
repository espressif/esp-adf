# GFH and GFZ atlas formats

This note describes the on-disk layouts and encode/decode algorithms for the
built-in compact-font atlases. Multi-byte integers are little-endian.
Tools: [tools/mk_gfh.py](tools/mk_gfh.py), [tools/mk_gfz.py](tools/mk_gfz.py).
Device codecs: [src/esp_cf_gfh.c](src/esp_cf_gfh.c), [src/esp_cf_gfz.c](src/esp_cf_gfz.c),
[src/gfh_dec.c](src/gfh_dec.c).

Both formats share the same 28-byte header shape and the same glyph index.
They differ only in how each glyph body is stored.

---

## Shared header (28 bytes)

| Offset | Size | Field | Notes |
| ------ | ---- | ----- | ----- |
| 0 | 4 | `magic` | `'GFH1'` = `0x31484647`, or `'GFZ1'` = `0x315A4647` |
| 4 | 2 | `px` | Base pixel size stored in the file |
| 6 | 2 | `flags` | See below |
| 8 | 4 | `count` | Number of glyphs |
| 12 | 4 | `index_off` | Byte offset of the glyph index |
| 16 | 4 | `lens_off` / `dict_off` | GFH: Huffman code lengths. GFZ: zstd dictionary |
| 20 | 4 | `lens_len` / `dict_len` | GFH: always 256. GFZ: dictionary byte length |
| 24 | 4 | `data_off` | Byte offset of the packed glyph payload |

### Flags

| Bit | Name | Meaning |
| --- | ---- | ------- |
| 0 | `PACK2` | Legacy 2 bpp marker. GFH: alone means bpp = 2. GFZ: required |
| 1 | `IDX8` | Index entries are 16 bytes (int8 metrics). Clear means the older 20-byte index |
| 8..11 | bpp nibble | GFH only: 1, 2, or 4. Zero with `PACK2` also means 2 bpp |

Current encoders always set `IDX8`. New files should do the same.

### Glyph index

Glyphs are sorted by Unicode codepoint. `data_off` in each entry is relative
to the file's payload base (`hdr.data_off`).

**16-byte entry (`IDX8`):**

| Offset | Size | Type | Field |
| ------ | ---- | ---- | ----- |
| 0 | 4 | u32 | codepoint |
| 4 | 4 | u32 | payload offset from `data_off` |
| 8 | 2 | u16 | frame length in bytes |
| 10 | 1 | i8 | advance |
| 11 | 1 | i8 | left bearing |
| 12 | 1 | i8 | top bearing |
| 13 | 1 | i8 | width |
| 14 | 1 | i8 | height |
| 15 | 1 | — | pad |

Metrics must fit int8 (advance / width / height in 0..127, bearings in
-128..127). That matches the library's 80 px glyph cap.

**20-byte entry (legacy, no `IDX8`):**

| Offset | Size | Type | Field |
| ------ | ---- | ---- | ----- |
| 0 | 4 | u32 | codepoint |
| 4 | 2 | i16 | advance |
| 6 | 2 | i16 | left |
| 8 | 2 | i16 | top |
| 10 | 2 | u16 | width |
| 12 | 2 | u16 | height |
| 14 | 4 | u32 | payload offset |
| 18 | 2 | u16 | frame length |

A blank glyph has `w == h == 0` and may have `frame_len == 0`. Any inked
glyph must have a non-zero frame that stays inside the file.

---

## GFH1 — Huffman coverage atlas

Magic `'GFH1'`. Supports **1 / 2 / 4 bpp**. One Huffman table is shared by
every glyph in the file.

### File layout

```
[ header 28 B ]
[ index: count × 16 B ]
[ code lengths: 256 B ]
[ glyph frames concatenated ]
```

`lens_off` points at 256 bytes. Byte `i` is the Huffman code length of
symbol `i`. Zero means the symbol is unused. Lengths must not exceed 20.

### Encode pipeline

Starting from an A8 FreeType mask (`0..255` per pixel):

1. **Quantize** to `bpp` levels:
   `q = min(v * (1 << bpp) / 256, (1 << bpp) - 1)`.
2. **Vertical XOR** (delta from the row above). For each column, from the
   bottom row upward:
   `out[y][x] ^= out[y - 1][x]`.
   Row 0 is unchanged. This removes vertical runs of flat coverage.
3. **Pack symbols**. Each Huffman symbol is one byte holding `8 / bpp`
   quantized pixels. The lowest pixel sits in the low bits:
   - 1 bpp → 8 pixels per symbol
   - 2 bpp → 4 pixels per symbol
   - 4 bpp → 2 pixels per symbol
4. **Build one Huffman tree** over symbol frequencies across the whole
   atlas. Canonical codes are assigned by sorting `(length, symbol)`.
5. **Bitstream**. Codes are written MSB-first into a byte stream. The last
   byte is padded with zeros on the right.

### Decode pipeline

On bind, the device reads the 256 lengths, builds a 12-bit primary peek
table (and a secondary table when some codes are longer than 12 bits),
then keeps those tables for the life of the font.

For each glyph frame:

1. Read Huffman symbols until `w * h` pixels are produced.
2. Unpack `8 / bpp` indices from each symbol.
3. **Undo vertical XOR**: each pixel is XORed with the previous row's
   reconstructed index in the same column (`prev` starts as zeros).
4. **Expand to A8** with fixed opacity tables:

| bpp | Levels |
| --- | ------ |
| 1 | `0, 255` |
| 2 | `0, 85, 170, 255` |
| 4 | `0, 17, 34, …, 255` (16 steps) |

Output is a tight `w * h` A8 mask. Metrics come from the index, not the
bitstream.

### Why GFH

- One table for the whole atlas → small open cost, fast per-glyph decode
- Vertical XOR + Huffman beats plain RLE on dense CJK coverage
- No per-glyph dictionary or inflate context

---

## GFZ1 — zstd coverage atlas

Magic `'GFZ1'`. **2 bpp only**. Each glyph is an independent zstd frame that
shares one dictionary stored in the file. Enable with
`CONFIG_ESP_COMPACT_FONT_GFZ`.

### File layout

```
[ header 28 B ]
[ index: count × 16 B ]
[ dictionary: dict_len B, starts with magic 0xEC30A437 ]
[ glyph frames concatenated ]
```

`dict_off` / `dict_len` replace GFH's length table fields. The first four
bytes of the dictionary blob must be `0xEC30A437` (zstd dictionary magic).
The rest is a raw zstd compression dictionary (`ZSTD_CDict` bytes from
training).

### Encode pipeline

1. **Quantize** A8 to 2 bpp: `q = min(v * 4 / 256, 3)`.
2. **Pack** four indices per byte, low pixel in the low bits (same packing
   as GFH 2 bpp symbols, but without XOR or Huffman).
3. **Train** one dictionary (default 16 KiB) on the packed glyph samples.
4. **Compress** each packed glyph as its own zstd frame with that
   dictionary. Frames omit checksum, content size, and dict id so the
   header stays small (encoder level 19 in `mk_gfz.py`).

### Decode pipeline

On bind, the device keeps a pointer to the in-file dictionary and a static
zstd `DCtx` workspace (via `retain`). For each glyph:

1. `ZSTD_decompress_usingDict` into the caller's buffer (capacity at least
   `(w * h + 3) / 4` bytes).
2. **Expand in place** from packed 2-bit indices to A8 using
   `{0, 85, 170, 255}`, walking pixels from the end so the expansion can
   overwrite the packed bytes safely.

### Why GFZ

- Per-glyph frames → decode only the glyphs you draw
- Shared dictionary → better ratio on large CJK sets than independent zstd
- Heavier than GFH (inflate + dictionary workspace); keep it off unless
  the size win matters

---

## Side-by-side

| | GFH1 | GFZ1 |
| --- | --- | --- |
| Magic | `GFH1` | `GFZ1` |
| bpp | 1 / 2 / 4 | 2 only |
| Shared table | Huffman lengths (256 B) | zstd dictionary |
| Per-glyph body | Huffman bitstream | zstd frame |
| Prediction | Vertical XOR | None |
| Kconfig | always linked | `ESP_COMPACT_FONT_GFZ` |
| Typical use | Default atlas | Optional denser 2 bpp store |

Both expose the same public metrics and A8 output through
`esp_compact_font_*`. Runtime pixel scaling (`esp_compact_font_set_pixel`)
is outside the file format: the atlas stays at `px`, and the library
resamples the decoded mask.
