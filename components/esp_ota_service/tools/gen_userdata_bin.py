#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# Generate userdata.bin for ota_fs: 4-byte little-endian semver header + fill.
# Packing matches ota_version_pack_semver() / app version "x.y.z" rules.

from __future__ import annotations

import argparse
import struct
import sys


def pack_semver(version: str) -> int:
    s = version.strip()
    if s[:1].lower() == 'v':
        s = s[1:]
    parts = s.split('.')
    if len(parts) != 3:
        raise ValueError(f'expected semver x.y.z, got {version!r}')
    maj, mi, pa = (int(p, 10) for p in parts)
    for n, label in ((maj, 'major'), (mi, 'minor'), (pa, 'patch')):
        if not 0 <= n <= 255:
            raise ValueError(f'{label} must be 0..255, got {n}')
    return (maj << 16) | (mi << 8) | pa


def main() -> int:
    p = argparse.ArgumentParser(
        description='Write userdata.bin with LE u32 semver header (ota_fs / ota_version_pack_semver).'
    )
    p.add_argument('version', help='Semver string, e.g. 0.2.0 (same style as version.txt / esp_app_desc)')
    p.add_argument('-o', '--output', default='userdata.bin', help='Output path')
    p.add_argument(
        '-s',
        '--size',
        type=int,
        default=1024,
        metavar='N',
        help='Total file size in bytes (default: 1024)',
    )
    args = p.parse_args()
    if args.size < 4:
        p.error('size must be at least 4')

    try:
        packed = pack_semver(args.version)
    except ValueError as e:
        print(f'gen_userdata_bin: {e}', file=sys.stderr)
        return 1

    body = bytearray(args.size)
    struct.pack_into('<I', body, 0, packed & 0xFFFFFFFF)
    for i in range(4, len(body)):
        body[i] = i & 0xFF

    with open(args.output, 'wb') as f:
        f.write(body)

    print(f'Wrote {args.output}: {args.size} bytes, LE u32=0x{packed & 0xFFFFFFFF:08x} ({args.version})')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
