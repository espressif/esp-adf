#!/usr/bin/env python3
"""Build a GFZ1 atlas. Optional on the device: enable ESP_COMPACT_FONT_GFZ.

2-bit coverage (reconstructed 0/85/170/255), one independent zstd frame per
glyph, and a 16 KB dictionary shared by that weight. Checksum, content size,
and dict id are omitted so the frame stays small. Encode runs here; the
reader only inflates.

  python3 components/esp_compact_font/tools/mk_gfz.py

Writes NotoSansCJK.gfz and NotoSansCJK-Bold.gfz at the repo root.
"""
from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import zstandard as zstd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from raster import EXTRA, ensure_raster, gb2312_codepoints, parse_ra8  # noqa: E402

GFZ_MAGIC = 0x315A4647  # 'GFZ1'
GFZ_FLAG_PACK2 = 1
GFZ_FLAG_IDX8 = 2
GFZ_HDR = 28
GFZ_ENT = 16
DICT_SIZE = 16 * 1024
LEVEL = 19

ROOT = Path(__file__).resolve().parents[3]
FONTS = Path('/usr/share/fonts/opentype/noto')
JOBS = (
    (FONTS / 'NotoSansCJK-Regular.ttc', ROOT / 'NotoSansCJK.gfz'),
    (FONTS / 'NotoSansCJK-Bold.ttc', ROOT / 'NotoSansCJK-Bold.gfz'),
)


def pack_2bit(alpha: bytes) -> bytes:
    n = len(alpha)
    out = bytearray((n + 3) // 4)
    acc = 0
    shift = 0
    j = 0
    for v in alpha:
        q = v * 4 // 256
        if q > 3:
            q = 3
        acc |= q << shift
        shift += 2
        if shift == 8:
            out[j] = acc
            j += 1
            acc = 0
            shift = 0
    if shift:
        out[j] = acc
    return bytes(out)


def train_dict(samples: list[bytes]):
    ss = [s for s in samples if len(s) >= 8]
    if len(ss) > 4000:
        step = len(ss) / 4000
        ss = [ss[int(i * step)] for i in range(4000)]
    try:
        return zstd.train_dictionary(DICT_SIZE, ss, k=100, d=8, threads=-1, level=6)
    except TypeError:
        return zstd.train_dictionary(DICT_SIZE, ss)


def write_gfz(out_path: Path, px: int, ents: list) -> None:
    packed = [pack_2bit(alpha) for *_, alpha in ents]
    print(f'  training {DICT_SIZE // 1024} KB dict on {min(len(packed), 4000)} samples', flush=True)
    dic = train_dict(packed)
    raw_dict = dic.as_bytes()
    cctx = zstd.ZstdCompressor(
        level=LEVEL,
        dict_data=dic,
        write_checksum=False,
        write_content_size=False,
        write_dict_id=False,
    )
    frames: list[bytes] = []
    for i, blob in enumerate(packed):
        frames.append(cctx.compress(blob) if blob else b'')
        if i and i % 2000 == 0:
            print(f'  compressed {i}/{len(packed)}', flush=True)

    dctx = zstd.ZstdDecompressor(dict_data=zstd.ZstdCompressionDict(raw_dict))
    for (cp, _adv, _left, _top, w, h, _alpha), blob, frame in zip(ents, packed, frames):
        if not blob:
            continue
        got = dctx.decompress(frame, max_output_size=len(blob))
        if got != blob:
            raise SystemExit(f'round-trip mismatch U+{cp:04X} {w}x{h}')

    index_off = GFZ_HDR
    dict_off = index_off + len(ents) * GFZ_ENT
    data_off = dict_off + len(raw_dict)
    index = bytearray()
    parts: list[bytes] = []
    cursor = 0
    for (cp, adv, left, top, w, h, _alpha), frame in zip(ents, frames):
        if len(frame) > 0xFFFF:
            raise SystemExit(f'frame longer than 65535 for U+{cp:04X}')
        if not (0 <= adv <= 127 and 0 <= w <= 127 and 0 <= h <= 127 and
                -128 <= left <= 127 and -128 <= top <= 127):
            raise SystemExit(
                f'U+{cp:04X} metrics adv={adv} left={left} top={top} {w}x{h} '
                f'do not fit an 80 px int8 index'
            )
        index += struct.pack('<IIHbbbbbx', cp, cursor, len(frame), adv, left, top, w, h)
        parts.append(frame)
        cursor += len(frame)
    hdr = struct.pack(
        '<IHHIIIII',
        GFZ_MAGIC,
        px,
        GFZ_FLAG_PACK2 | GFZ_FLAG_IDX8,
        len(ents),
        index_off,
        dict_off,
        len(raw_dict),
        data_off,
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(hdr + bytes(index) + raw_dict + b''.join(parts))
    kb = out_path.stat().st_size / 1024
    print(
        f'wrote {out_path.name}: {len(ents)} glyphs, {px}px, '
        f'dict {len(raw_dict)} B, file {kb:.1f} KB',
        flush=True,
    )


def raster(mk: Path, font: Path, px: int, cps_path: Path, out: Path) -> None:
    subprocess.check_call([str(mk), str(font), str(px), str(out), str(cps_path)])


def main() -> None:
    tools = Path(__file__).resolve().parent
    mk = ensure_raster(tools)
    cps = set(EXTRA)
    cps.update(gb2312_codepoints())
    px = 36
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        cps_path = td_path / 'cps.txt'
        cps_path.write_text('\n'.join(f'0x{cp:X}' for cp in sorted(cps)))
        for font, dest in JOBS:
            if not font.is_file():
                raise SystemExit(f'missing font {font}')
            raw = td_path / (dest.stem + '.ra8')
            print(f'raster {font.name} at {px}px', flush=True)
            raster(mk, font, px, cps_path, raw)
            got_px, ents = parse_ra8(raw.read_bytes())
            if got_px != px:
                raise SystemExit(f'{font.name} raster px {got_px}')
            raw.unlink()
            write_gfz(dest, got_px, ents)


if __name__ == '__main__':
    main()
