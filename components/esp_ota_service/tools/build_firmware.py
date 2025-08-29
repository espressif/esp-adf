#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# Build one OTA example and publish its output as a versioned artefact in
# tools/firmware_samples/, where every downstream tool (sdcard_writer HTTP
# server, BLE uploader, HIL automation) picks its payload from.
#
# Usage
# -----
#   # Build examples/ota_http at the version already written in version.txt
#   ./build_firmware.py http
#
#   # Bump version.txt first, then build; publish as ota_fs_v0.3.0.bin plus
#   # a matching userdata_v0.3.0.bin (fs-only).
#   ./build_firmware.py fs --version 0.3.0
#
#   # Override target / port for idf.py
#   ./build_firmware.py ble -v 1.0.1 --target esp32s3 --port /dev/ttyUSB0
#
# The script always runs `idf.py build` under the example directory; it does
# NOT flash. Output artefacts:
#   firmware_samples/<project>_v<version>.bin     (the OTA payload)
#   firmware_samples/userdata_v<version>.bin      (fs example only)
#   firmware_samples/manifest.json                (refreshed to point at the
#                                                  just-published version)

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from _fw_common import (
    EXAMPLES,
    FIRMWARE_SAMPLES_DIR,
    example_dir,
    hash_file,
    load_manifest,
    pack_semver_u32,
    parse_semver,
    save_manifest,
    versioned_name,
)


def write_version_txt(ex_dir: Path, version: str) -> Path:
    parse_semver(version)
    version_path = ex_dir / 'version.txt'
    version_path.write_text(version + '\n')
    return version_path


def read_version_txt(ex_dir: Path) -> str:
    version_path = ex_dir / 'version.txt'
    if not version_path.is_file():
        raise FileNotFoundError(
            f'{version_path} not found - pass --version to create it'
        )
    version = version_path.read_text().strip()
    parse_semver(version)
    return version


def run_idf_build(ex_dir: Path, target: str | None, port: str | None) -> None:
    env = os.environ.copy()
    cmd: list[str] = ['idf.py']
    if port:
        cmd += ['-p', port]
    cmd += ['build']
    if target:
        # idf.py handles set-target if sdkconfig doesn't match, but we let the
        # caller trigger it explicitly with --target.
        subprocess.check_call(['idf.py', 'set-target', target], cwd=ex_dir, env=env)
    print(f'+ (cwd {ex_dir}) {' '.join(cmd)}', flush=True)
    subprocess.check_call(cmd, cwd=ex_dir, env=env)


def publish_bin(src: Path, dst: Path) -> None:
    FIRMWARE_SAMPLES_DIR.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        dst.unlink()
    shutil.copy2(src, dst)
    print(f'[publish] {src.name} -> {dst}')


def write_userdata_bin(dst: Path, version: str, size: int = 1024) -> None:
    if size < 4:
        raise ValueError('userdata size must be >= 4')
    packed = pack_semver_u32(version)
    body = bytearray(size)
    struct.pack_into('<I', body, 0, packed & 0xFFFFFFFF)
    # Fill remainder with a deterministic pattern so that diffing two userdata
    # bins only flags the header change.
    for i in range(4, len(body)):
        body[i] = i & 0xFF
    dst.write_bytes(bytes(body))
    print(f'[publish] userdata ({size} B, LE u32=0x{packed:08x}) -> {dst}')


def refresh_manifest(example_key: str, version: str, fw_path: Path,
                     userdata_path: Path | None, url_override: str | None) -> None:
    info = EXAMPLES[example_key]
    stats = hash_file(fw_path)

    url = url_override
    if url is None:
        prev = load_manifest()
        if prev.get('project_name') == info['project_name'] and prev.get('url'):
            url = prev['url']
    if url is None:
        url = info['default_url']

    entry = {
        'example': example_key,
        'project_name': info['project_name'],
        'version': version,
        'file': fw_path.name,
        'size': stats.size,
        'sha256': stats.sha256,
        'md5': stats.md5,
        'url': url,
    }
    if userdata_path is not None:
        entry['userdata_file'] = userdata_path.name

    save_manifest(entry)
    print(f'[manifest] refreshed {FIRMWARE_SAMPLES_DIR / 'manifest.json'}')
    print(f'           version={version}  size={stats.size}  sha256={stats.sha256}')


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Build an OTA example and publish a versioned bin '
                    'into tools/firmware_samples/.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('example', choices=sorted(EXAMPLES.keys()),
                        help='Which OTA example to build: http | fs | ble.')
    parser.add_argument('-v', '--version', default=None,
                        help='Semver string to write into version.txt before '
                             'building. Omit to reuse the current version.txt.')
    parser.add_argument('-u', '--url', default=None,
                        help='URL to record in manifest.json (defaults to the '
                             'previous manifest url for the same example, else '
                             "the example's built-in default).")
    parser.add_argument('--target', default=None,
                        help='Invoke idf.py set-target <chip> before build.')
    parser.add_argument('--port', default=None,
                        help='Pass -p <port> to idf.py (only used for logging / '
                             'esp-idf port-sensitive steps).')
    parser.add_argument('--userdata-size', type=int, default=1024,
                        help='userdata.bin size in bytes for the fs example '
                             '(default 1024).')
    parser.add_argument('--skip-build', action='store_true',
                        help='Skip idf.py build; only (re)publish the bin that '
                             'is already in build/.')
    args = parser.parse_args()

    ex_dir = example_dir(args.example)
    if not ex_dir.is_dir():
        print(f'[error] example dir missing: {ex_dir}', file=sys.stderr)
        return 1

    if args.version:
        try:
            version = args.version.strip().lstrip('vV')
            parse_semver(version)
        except ValueError as e:
            print(f'[error] bad --version: {e}', file=sys.stderr)
            return 1
        write_version_txt(ex_dir, version)
    else:
        try:
            version = read_version_txt(ex_dir)
        except (FileNotFoundError, ValueError) as e:
            print(f'[error] {e}', file=sys.stderr)
            return 1

    if not args.skip_build:
        try:
            run_idf_build(ex_dir, args.target, args.port)
        except subprocess.CalledProcessError as e:
            return e.returncode or 1

    info = EXAMPLES[args.example]
    build_bin = ex_dir / 'build' / f'{info['project_name']}.bin'
    if not build_bin.is_file():
        print(f'[error] missing build output: {build_bin}\n'
              f'        Run without --skip-build, or build manually first.',
              file=sys.stderr)
        return 1

    fw_dst = FIRMWARE_SAMPLES_DIR / versioned_name(info['project_name'], version)
    publish_bin(build_bin, fw_dst)

    userdata_dst: Path | None = None
    if args.example == 'fs':
        userdata_dst = FIRMWARE_SAMPLES_DIR / f'userdata_v{version}.bin'
        write_userdata_bin(userdata_dst, version, args.userdata_size)

    refresh_manifest(args.example, version, fw_dst, userdata_dst, args.url)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
