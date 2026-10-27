#!/usr/bin/env python3
"""Download a free OFL Chinese font, subset it to at most 2 MiB, and embed it.

The 2 MiB cap is a real subset (pyftsubset). Cutting the file with head
would leave a broken font.

LVGL bins come from lv_font_conv, the offline form of
https://lvgl.io/tools/fontconverter. Compression is left at the tool
default: on for 2-bit and 4-bit, off for 1-bit. That is the site's
"Enable Font compression" switch. gfh_1bpp.gfh, gfh_2bpp.gfh, and
gfh_4bpp.gfh are this component's atlases of the same codepoints.

Outputs land in fonts/ next to this script: lvgl_*.bin, gfh_*.gfh, and
gfz_2bpp.gfz. A file is kept when its header and size look like a real
atlas. An empty or truncated file is rebuilt. --force rebuilds all of them.
"""
from __future__ import annotations

import argparse
import io
import struct
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parents[1] / 'tools'
CACHE = HERE / '.cache'
FONTS = HERE / 'fonts'
LIMIT = 2 * 1024 * 1024
PX = 36
MIN_ATLAS = 64 * 1024
GFH_MAGIC = 0x31484647
GFZ_MAGIC = 0x315A4647

sys.path.insert(0, str(TOOLS))
from mk_gfh import write_gfh  # noqa: E402
from raster import EXTRA, ensure_raster, parse_ra8  # noqa: E402

URLS = (
    'https://github.com/notofonts/noto-cjk/raw/main/Sans/Subset/OTF/SC/NotoSansCJKsc-Regular.otf',
    'https://github.com/adobe-fonts/source-han-sans/raw/release/SubsetOTF/SC/SourceHanSansSC-Regular.otf',
    'https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf',
)
LOCAL_TTC = Path('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')
CJK_END = 0x9FFF


def font_kind(path: Path) -> bytes:
    if not path.is_file() or path.stat().st_size < 100_000:
        return b''
    return path.read_bytes()[:4]


def download(dest: Path) -> tuple[Path, bool]:
    kind = font_kind(dest)
    if kind in (b'OTTO', b'\x00\x01\x00\x00', b'ttcf'):
        print(f'font cache {dest} ({dest.stat().st_size} bytes)', flush=True)
        return dest, kind == b'ttcf'
    dest.parent.mkdir(parents=True, exist_ok=True)
    for url in URLS:
        print(f'download {url}', flush=True)
        try:
            req = urllib.request.Request(url, headers={'User-Agent': 'esp-compact-font'})
            with urllib.request.urlopen(req, timeout=120) as resp, dest.open('wb') as out:
                while True:
                    chunk = resp.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
        except Exception as exc:
            print(f'  failed: {exc}', flush=True)
            dest.unlink(missing_ok=True)
            continue
        head = dest.read_bytes()[:4] if dest.is_file() else b''
        if dest.is_file() and dest.stat().st_size > 100_000 and head in (
            b'OTTO', b'\x00\x01\x00\x00', b'ttcf'
        ):
            print(f'  saved {dest.stat().st_size} bytes', flush=True)
            return dest, head == b'ttcf'
        dest.unlink(missing_ok=True)
    if LOCAL_TTC.is_file():
        print(f'download failed; using {LOCAL_TTC} face 0', flush=True)
        return LOCAL_TTC, True
    raise SystemExit('no Chinese font available')


def unicode_spec(n: int) -> str:
    parts = []
    if n > 0:
        parts.append(f'4E00-{0x4E00 + n - 1:X}')
    extra = sorted(c for c in EXTRA if c < 0x4E00 or c > CJK_END)
    if extra:
        parts.append(','.join(f'{c:X}' for c in extra))
    return ','.join(parts)


def subset(src: Path, dest: Path, n: int, ttc: bool) -> int:
    cmd = [
        'pyftsubset',
        str(src),
        f'--unicodes={unicode_spec(n)}',
        f'--output-file={dest}',
        '--no-hinting',
        '--layout-features=',
        '--glyph-names',
        '--symbol-cmap',
        '--legacy-cmap',
        '--notdef-glyph',
        '--notdef-outline',
        '--recommended-glyphs',
        '--name-IDs=*',
        '--name-legacy',
        '--name-languages=*',
    ]
    if ttc:
        cmd.append('--font-number=0')
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
    return dest.stat().st_size


def largest_subset(src: Path, dest: Path, ttc: bool) -> int:
    lo, hi = 1, CJK_END - 0x4E00 + 1
    best = 0
    while lo <= hi:
        n = (lo + hi) // 2
        size = subset(src, dest, n, ttc)
        print(f'  subset U+4E00+{n} -> {size} bytes', flush=True)
        if size <= LIMIT:
            best = n
            lo = n + 1
        else:
            hi = n - 1
    if best <= 0:
        raise SystemExit('even a short subset exceeds 2 MiB')
    size = subset(src, dest, best, ttc)
    print(f'cut {best} CJK ideographs, {size} bytes (limit {LIMIT})', flush=True)
    return best


LV_FONT_CONV_TGZ = 'https://registry.npmjs.org/lv_font_conv/-/lv_font_conv-1.5.3.tgz'
FT_RENDER_MEMBER = 'package/lib/freetype/build/ft_render.js'


def lv_font_conv() -> Path:
    root = CACHE / 'node_modules' / 'lv_font_conv'
    bindir = CACHE / 'node_modules' / '.bin' / 'lv_font_conv'
    ft_render = root / 'lib' / 'freetype' / 'build' / 'ft_render.js'
    if not bindir.is_file():
        CACHE.mkdir(parents=True, exist_ok=True)
        subprocess.check_call(
            ['npm', 'install', '--silent', 'lv_font_conv@1.5.3'],
            cwd=CACHE,
        )
    # npm can leave this install without lib/freetype/build. The published
    # 1.5.3 tarball contains the FreeType wasm helper that index.js requires.
    if not ft_render.is_file():
        print(f'fetch {FT_RENDER_MEMBER} from lv_font_conv 1.5.3', flush=True)
        req = urllib.request.Request(LV_FONT_CONV_TGZ, headers={'User-Agent': 'esp-compact-font'})
        with urllib.request.urlopen(req, timeout=120) as resp:
            blob = resp.read()
        with tarfile.open(fileobj=io.BytesIO(blob), mode='r:gz') as tar:
            member = tar.getmember(FT_RENDER_MEMBER)
            extracted = tar.extractfile(member)
            if extracted is None:
                raise SystemExit(f'{FT_RENDER_MEMBER} missing from lv_font_conv tarball')
            ft_render.parent.mkdir(parents=True, exist_ok=True)
            ft_render.write_bytes(extracted.read())
    if not bindir.is_file() or not ft_render.is_file():
        raise SystemExit('lv_font_conv did not install')
    return bindir


def hdr_ok(path: Path, magic: int) -> bool:
    if not path.is_file() or path.stat().st_size < MIN_ATLAS:
        return False
    with path.open('rb') as handle:
        blob = handle.read(28)
    if len(blob) < 28:
        return False
    mag, px, _flags, count, index_off, _mid, _extra, data_off = struct.unpack('<IHHIIIII', blob)
    size = path.stat().st_size
    return mag == magic and px > 0 and count >= 100 and 0 <= index_off < size and 0 < data_off < size


def lvgl_ok(path: Path) -> bool:
    if not path.is_file() or path.stat().st_size < MIN_ATLAS:
        return False
    with path.open('rb') as handle:
        blob = handle.read(8)
    return len(blob) >= 8 and blob[4:8] == b'head'


def font_ok(path: Path) -> bool:
    name = path.name
    if name.endswith('.gfh'):
        return hdr_ok(path, GFH_MAGIC)
    if name.endswith('.gfz'):
        return hdr_ok(path, GFZ_MAGIC)
    if name.endswith('.bin'):
        return lvgl_ok(path)
    return False


def convert_lvgl(conv: Path, face: Path, n: int) -> None:
    end = 0x4E00 + n - 1
    for bpp in (1, 2, 4):
        out = FONTS / f'lvgl_{bpp}bpp.bin'
        if font_ok(out):
            print(f'keep {out.name} ({out.stat().st_size} bytes)', flush=True)
            continue
        cmd = [
            str(conv),
            '--font', str(face),
            '--size', str(PX),
            '--bpp', str(bpp),
            '--format', 'bin',
            '--no-kerning',
            '--range', f'0x4E00-0x{end:X}',
            '-o', str(out),
        ]
        for cp in sorted(c for c in EXTRA if c < 0x4E00 or c > CJK_END):
            cmd.extend(['--range', f'0x{cp:X}'])
        print(' '.join(cmd), flush=True)
        subprocess.check_call(cmd)


def cut_span(path: Path) -> int:
    from fontTools.ttLib import TTFont

    cmap = TTFont(str(path)).getBestCmap() or {}
    hans = [cp for cp in cmap if 0x4E00 <= cp <= CJK_END]
    if not hans:
        raise SystemExit(f'{path} has no CJK')
    return max(hans) - 0x4E00 + 1


def convert_gfh(face: Path, n: int) -> None:
    missing = [b for b in (1, 2, 4) if not font_ok(FONTS / f'gfh_{b}bpp.gfh')]
    gfz = FONTS / 'gfz_2bpp.gfz'
    need_gfz = not font_ok(gfz)
    if not missing and not need_gfz:
        print('keep gfh_1bpp.gfh gfh_2bpp.gfh gfh_4bpp.gfh gfz_2bpp.gfz', flush=True)
        return
    cps = list(range(0x4E00, 0x4E00 + n))
    cps.extend(c for c in EXTRA if c < 0x4E00 or c > CJK_END)
    cps_path = CACHE / 'cps.txt'
    cps_path.write_text('\n'.join(f'0x{c:X}' for c in sorted(set(cps))))
    raw = CACHE / 'subset.ra8'
    raster = ensure_raster(TOOLS)
    print(f'raster {face.name} at {PX}px', flush=True)
    subprocess.check_call(
        [str(raster), str(face), str(PX), str(raw), str(cps_path)],
        stdout=subprocess.DEVNULL,
    )
    got_px, ents = parse_ra8(raw.read_bytes())
    raw.unlink(missing_ok=True)
    if got_px != PX:
        raise SystemExit(f'raster px {got_px}')
    for bpp in missing:
        write_gfh(FONTS / f'gfh_{bpp}bpp.gfh', got_px, ents, bpp)
    if need_gfz:
        from mk_gfz import write_gfz  # noqa: E402

        write_gfz(gfz, got_px, ents)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    FONTS.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    outs = [FONTS / f'lvgl_{b}bpp.bin' for b in (1, 2, 4)]
    outs += [FONTS / f'gfh_{b}bpp.gfh' for b in (1, 2, 4)]
    outs.append(FONTS / 'gfz_2bpp.gfz')
    if args.force:
        for p in outs:
            p.unlink(missing_ok=True)
    else:
        bad = [p for p in outs if not font_ok(p)]
        if not bad:
            print('fonts already prepared', flush=True)
            for p in outs:
                print(f'  {p.name} {p.stat().st_size} bytes', flush=True)
            return
        for p in bad:
            size = p.stat().st_size if p.is_file() else 0
            print(f'regenerate {p.name}: size {size} is missing or abnormal', flush=True)
            p.unlink(missing_ok=True)
    src, ttc = download(CACHE / 'source.otf')
    cut = CACHE / 'cut.otf'
    if cut.is_file() and cut.stat().st_size > 0 and not args.force:
        n = cut_span(cut)
        print(f'keep {cut.name}: {n} ideographs, {cut.stat().st_size} bytes', flush=True)
    else:
        n = largest_subset(src, cut, ttc)
    convert_lvgl(lv_font_conv(), cut, n)
    convert_gfh(cut, n)


if __name__ == '__main__':
    main()
