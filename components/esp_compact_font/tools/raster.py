"""Shared codepoint list and raw-alpha reader for the GFH and GFZ encoders."""
from __future__ import annotations

import struct
import subprocess
from pathlib import Path

RA8_MAGIC = 0x31414852  # 'RA81'

EXTRA = [
    0x0020,
    0x3000, 0x3001, 0x3002, 0x300A, 0x300B, 0x3010, 0x3011,
    0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x00B7,
    0xFF01, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F,
]


def gb2312_codepoints() -> list[int]:
    out: list[int] = []
    for zone in range(0xA1, 0xF8):
        for num in range(0xA1, 0xFF):
            try:
                out.append(ord(bytes([zone, num]).decode('gb2312')))
            except Exception:
                pass
    return sorted(set(out))


def parse_ra8(data: bytes):
    magic, px, _pad, count = struct.unpack_from('<IHHI', data, 0)
    if magic != RA8_MAGIC:
        raise ValueError('not an RA81 raster dump')
    off = 12
    ents = []
    for _ in range(count):
        cp, adv, left, top, w, h = struct.unpack_from('<IhhhHH', data, off)
        off += 14
        raw = w * h
        alpha = data[off : off + raw]
        if len(alpha) != raw:
            raise ValueError(f'glyph U+{cp:04X} truncated')
        off += raw
        ents.append((cp, adv, left, top, w, h, alpha))
    return px, ents


def ensure_raster(tools: Path) -> Path:
    src = tools / 'raster_a8.c'
    bin_path = tools / 'raster_a8'
    cflags = subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'freetype2'], text=True
    ).split()
    subprocess.check_call(['gcc', '-O2', '-o', str(bin_path), str(src), *cflags])
    return bin_path
