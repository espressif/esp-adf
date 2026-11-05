#!/usr/bin/env python3
"""Build a GFH1 atlas: 1/2/4-bit pixels, vertical XOR, Huffman of 8-bit symbols.

The default files are 2-bit (4 pixels per symbol). 1-bit packs 8 pixels and
4-bit packs 2, so the code table stays 256 symbols.

One code-length table per file. Each glyph is its own bitstream, so a draw
decodes that glyph only. The reader builds a 12-bit lookup from the lengths.

  python3 components/esp_compact_font/tools/mk_gfh.py

Writes NotoSansCJK.gfh and NotoSansCJK-Bold.gfh at the repo root.
Copy them into the atlas folder (fonts/atlas) and the boot bench will time them.
"""
from __future__ import annotations

import heapq
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from raster import EXTRA, ensure_raster, gb2312_codepoints, parse_ra8  # noqa: E402

GFH_MAGIC = 0x31484647  # 'GFH1'
GFH_FLAG_PACK2 = 1
GFH_FLAG_IDX8 = 2
GFH_HDR = 28
GFH_ENT = 16
GFH_NSYM = 256
ROOT = Path(__file__).resolve().parents[3]
FONTS = Path('/usr/share/fonts/opentype/noto')
JOBS = (
    (FONTS / 'NotoSansCJK-Regular.ttc', ROOT / 'NotoSansCJK.gfh'),
    (FONTS / 'NotoSansCJK-Bold.ttc', ROOT / 'NotoSansCJK-Bold.gfh'),
)


def quantize(alpha: bytes, bpp: int) -> bytearray:
    maxv = (1 << bpp) - 1
    out = bytearray(len(alpha))
    for i, v in enumerate(alpha):
        q = v * (1 << bpp) // 256
        if q > maxv:
            q = maxv
        out[i] = q
    return out


def xor_up(idx: bytearray, w: int, h: int) -> bytearray:
    out = bytearray(idx)
    for y in range(h - 1, 0, -1):
        row = y * w
        prev = row - w
        for x in range(w):
            out[row + x] ^= out[prev + x]
    return out


def symbols(xored: bytes, bpp: int) -> list[int]:
    span = 8 // bpp
    out = []
    n = len(xored)
    i = 0
    while i < n:
        sym = 0
        for k in range(span):
            v = xored[i + k] if i + k < n else 0
            sym |= v << (k * bpp)
        out.append(sym)
        i += span
    return out


def code_lengths(counts: list[int]) -> bytes:
    heap = []
    for sym, freq in enumerate(counts):
        if freq:
            heap.append([freq, sym, None, None])
    if not heap:
        raise SystemExit('no symbols')
    if len(heap) == 1:
        lens = bytearray(GFH_NSYM)
        lens[heap[0][1]] = 1
        return bytes(lens)
    heapq.heapify(heap)
    while len(heap) > 1:
        a = heapq.heappop(heap)
        b = heapq.heappop(heap)
        heapq.heappush(heap, [a[0] + b[0], -1, a, b])
    lens = bytearray(GFH_NSYM)

    def walk(node, depth: int) -> None:
        if node[2] is None and node[3] is None:
            lens[node[1]] = depth or 1
            return
        walk(node[2], depth + 1)
        walk(node[3], depth + 1)

    walk(heap[0], 0)
    if max(lens) > 20:
        raise SystemExit(f'huffman length {max(lens)} exceeds 20')
    return bytes(lens)


def assign_codes(lens: bytes) -> dict[int, tuple[int, int]]:
    order = sorted((lens[s], s) for s in range(GFH_NSYM) if lens[s])
    code = 0
    prev = 0
    out = {}
    for L, s in order:
        code <<= L - prev
        out[s] = (code, L)
        code += 1
        prev = L
    return out


def encode_glyph(syms: list[int], codes: dict[int, tuple[int, int]]) -> bytes:
    acc = 0
    nbits = 0
    out = bytearray()
    for s in syms:
        c, L = codes[s]
        acc = (acc << L) | c
        nbits += L
        while nbits >= 8:
            nbits -= 8
            out.append((acc >> nbits) & 0xFF)
    if nbits:
        out.append((acc << (8 - nbits)) & 0xFF)
    return bytes(out)


def write_gfh(out_path: Path, px: int, ents: list, bpp: int = 2) -> bytes:
    if bpp not in (1, 2, 4):
        raise SystemExit(f'gfh bpp {bpp}')
    maxv = (1 << bpp) - 1
    levels = tuple((v * 255 + maxv // 2) // maxv for v in range(maxv + 1))
    # bit0 alone keeps the original 2-bit files readable. bit1 selects the
    # 16-byte index whose five metrics are int8.
    flags = (GFH_FLAG_PACK2 if bpp == 2 else (bpp << 8)) | GFH_FLAG_IDX8
    ents = sorted(ents, key=lambda e: e[0])
    prepared = []
    counts = [0] * GFH_NSYM
    for cp, adv, left, top, w, h, alpha in ents:
        idx = quantize(alpha, bpp)
        xored = xor_up(idx, w, h)
        syms = symbols(xored, bpp)
        for s in syms:
            counts[s] += 1
        prepared.append((cp, adv, left, top, w, h, idx, syms))
    lens = code_lengths(counts)
    codes = assign_codes(lens)
    frames = []
    expect = bytearray()
    for cp, adv, left, top, w, h, idx, syms in prepared:
        frame = encode_glyph(syms, codes)
        if (w > 0 and not frame) or len(frame) > 0xFFFF:
            raise SystemExit(f'bad frame U+{cp:04X} len {len(frame)}')
        frames.append(frame)
        # Expected bytes are the XOR-decoded indices expanded to alpha.
        # xor_up is inverted by XORing each row with the row above, which is
        # what the decoder does. `idx` is already the pre-XOR indices.
        expect += bytes(levels[v] for v in idx)

    index_off = GFH_HDR
    lens_off = index_off + len(prepared) * GFH_ENT
    data_off = lens_off + GFH_NSYM
    index = bytearray()
    cursor = 0
    parts = []
    for (cp, adv, left, top, w, h, _idx, _syms), frame in zip(prepared, frames):
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
        GFH_MAGIC,
        px,
        flags,
        len(prepared),
        index_off,
        lens_off,
        GFH_NSYM,
        data_off,
    )
    blob = hdr + bytes(index) + lens + b''.join(parts)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    print(
        f'wrote {out_path.name}: {len(prepared)} glyphs, {px}px, {bpp}-bit, '
        f'file {len(blob) / 1024:.1f} KB',
        flush=True,
    )
    return bytes(expect)


def raster(mk: Path, font: Path, px: int, cps_path: Path, out: Path) -> None:
    subprocess.check_call(
        [str(mk), str(font), str(px), str(out), str(cps_path)],
        stdout=subprocess.DEVNULL,
    )


def main() -> None:
    tools = Path(__file__).resolve().parent
    mk = ensure_raster(tools)
    cps = set(EXTRA)
    cps.update(gb2312_codepoints())
    px = 36
    dec = tools.parent / 'src' / 'gfh_dec.c'
    test = Path(tempfile.gettempdir()) / 'gfh_test'
    subprocess.check_call(
        ['gcc', '-O2', '-DGFH_DEC_TEST', '-I', str(dec.parent), '-o', str(test), str(dec)],
    )
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
            expect = write_gfh(dest, got_px, ents)
            exp_path = td_path / (dest.stem + '.exp')
            exp_path.write_bytes(expect)
            subprocess.check_call([str(test), str(dest), str(exp_path)])


if __name__ == '__main__':
    main()
