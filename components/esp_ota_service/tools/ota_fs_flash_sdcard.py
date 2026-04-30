#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# Build the tools/sdcard_writer IDF project with a versioned ota_fs payload
# (plus optional userdata.bin / flash_tone.bin) embedded, flash it to the
# board, then monitor the serial output until the writer reports SUCCESS.
#
# The writer boots, mounts the SD card through esp_board_manager, copies each
# embedded blob to the SD card under a stable name (app.bin / data.bin /
# flash_tone.bin), and verifies the first 16 bytes of each file.
#
# Typical use
# -----------
#   # Use the latest firmware_samples/ota_fs_v*.bin + userdata_v*.bin
#   ./ota_fs_flash_sdcard.py --port /dev/ttyUSB0
#
#   # Pin explicit versioned bins
#   ./ota_fs_flash_sdcard.py --app ota_fs_v0.2.0.bin --userdata userdata_v0.2.0.bin
#
#   # Skip monitor after flash (CI-friendly)
#   ./ota_fs_flash_sdcard.py --port /dev/ttyUSB0 --no-monitor

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

from _fw_common import (
    FIRMWARE_SAMPLES_DIR,
    TOOLS_DIR,
    latest_versioned_bin,
    resolve_bin,
)

SDCARD_WRITER_DIR = TOOLS_DIR / 'sdcard_writer'
BMGR_CODES_DIR = SDCARD_WRITER_DIR / 'components' / 'gen_bmgr_codes'
DEFAULT_BOARD = 'esp32_s3_korvo2_v3'
SUCCESS_LINE = b'*** SUCCESS:'
FAIL_LINE = b'*** FAILED:'


def resolve_optional_bin(stem_prefix: str, arg: str | None) -> Path | None:
    """Like resolve_bin, but returns None when --no-<x> was passed
    (value == 'none') or when nothing matches and the arg was omitted."""
    if arg and arg.lower() == 'none':
        return None
    if arg:
        return resolve_bin(stem_prefix, arg)
    return latest_versioned_bin(stem_prefix)


def ensure_bmgr_codes(board: str, env: dict) -> None:
    """Run `idf.py bmgr -b <board>` to make sure board_manager codes are
    present and the project sdkconfig has the matching peripheral/device
    Kconfigs enabled. bmgr itself is idempotent when the board is unchanged
    (it preserves sdkconfig in that case), and it rewrites missing Kconfig
    entries when set-target just regenerated the sdkconfig from defaults,
    so it is safe (and necessary) to run on every invocation."""
    print(f'+ (cwd {SDCARD_WRITER_DIR}) idf.py bmgr -b {board}', flush=True)
    subprocess.check_call(['idf.py', 'bmgr', '-b', board],
                          cwd=SDCARD_WRITER_DIR, env=env)


def build_and_flash(port: str | None, target: str | None, board: str,
                    app: Path, userdata: Path | None, tone: Path | None,
                    flash: bool) -> None:
    cache_defs = [f'-DOTA_FS_BIN={app}']
    if userdata is not None:
        cache_defs.append(f'-DOTA_FS_USERDATA_BIN={userdata}')
    if tone is not None:
        cache_defs.append(f'-DOTA_FS_TONE_BIN={tone}')

    env = os.environ.copy()

    if target:
        subprocess.check_call(['idf.py', 'set-target', target],
                              cwd=SDCARD_WRITER_DIR, env=env)

    ensure_bmgr_codes(board, env)

    cmd: list[str] = ['idf.py']
    if port:
        cmd += ['-p', port]
    cmd += cache_defs
    cmd += ['build']
    if flash:
        cmd += ['flash']

    print(f'+ (cwd {SDCARD_WRITER_DIR}) {' '.join(cmd)}', flush=True)
    subprocess.check_call(cmd, cwd=SDCARD_WRITER_DIR, env=env)


def monitor_until_result(port: str, overall_s: float) -> int:
    """Tail the serial port until we see the writer's SUCCESS / FAILED marker.
    Returns 0 on SUCCESS, 1 on FAILED, 2 on timeout.
    """
    try:
        import serial
    except ImportError:
        print('[warn] pyserial not installed; skipping monitor. '
              'Install with: pip install pyserial', file=sys.stderr)
        return 0

    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
    except serial.SerialException as e:
        print(f'[warn] cannot open {port}: {e}', file=sys.stderr)
        return 0

    print(f'[monitor] {port} @115200  (Ctrl+C to abort)', flush=True)
    buf = b''
    deadline = time.monotonic() + overall_s
    try:
        while time.monotonic() < deadline:
            chunk = ser.read(ser.in_waiting or 1)
            if not chunk:
                time.sleep(0.02)
                continue
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
            buf += chunk
            if len(buf) > 256 * 1024:
                buf = buf[-128 * 1024:]
            if SUCCESS_LINE in buf:
                return 0
            if FAIL_LINE in buf:
                return 1
    except KeyboardInterrupt:
        print('\n[monitor] aborted by user', flush=True)
    finally:
        try:
            ser.close()
        except serial.SerialException:
            pass
    return 2


def main() -> int:
    p = argparse.ArgumentParser(
        description='Build sdcard_writer with versioned firmware samples and '
                    'flash it so the target writes the payloads to SD.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument('--port', default=os.environ.get('ESPPORT'),
                   help='Serial port passed to idf.py / serial monitor.')
    p.add_argument('--target', default=None,
                   help='Invoke `idf.py set-target <chip>` first (e.g. esp32s3).')
    p.add_argument('--board', default=DEFAULT_BOARD,
                   help=f'Board for `idf.py bmgr -b <board>` '
                        f'(runs once, default: {DEFAULT_BOARD}).')
    p.add_argument('--app', default=None,
                   help='Firmware to embed as /sdcard/app.bin. '
                        "Path, filename inside firmware_samples/, or 'latest' "
                        '(default).')
    p.add_argument('--userdata', default=None,
                   help="userdata bin for /sdcard/data.bin (or 'none' to skip; "
                        'default: latest userdata_v*.bin).')
    p.add_argument('--tone', default=None,
                   help='flash_tone bin for /sdcard/flash_tone.bin '
                        "(or 'none' to skip; default: auto).")
    p.add_argument('--no-flash', action='store_true',
                   help='Only build the sdcard_writer project; do not flash.')
    p.add_argument('--no-monitor', action='store_true',
                   help='Skip serial monitor after flash.')
    p.add_argument('--monitor-timeout', type=float, default=180.0,
                   help='Seconds to wait for SUCCESS/FAILED line (default 180).')
    args = p.parse_args()

    try:
        app = resolve_bin('ota_fs', args.app)
    except FileNotFoundError as e:
        print(f'[error] {e}', file=sys.stderr)
        return 1

    try:
        userdata = resolve_optional_bin('userdata', args.userdata)
        tone = resolve_optional_bin('flash_tone', args.tone)
    except FileNotFoundError as e:
        print(f'[error] {e}', file=sys.stderr)
        return 1

    print(f'[bin] app      = {app}')
    print(f'[bin] userdata = {userdata or '(none)'}')
    print(f'[bin] tone     = {tone or '(none)'}')
    if userdata is None and args.userdata is None:
        print(f'       (no userdata_v*.bin in {FIRMWARE_SAMPLES_DIR}; '
              f'writer will skip /sdcard/data.bin)')

    do_flash = not args.no_flash
    if do_flash and not args.port:
        print('[error] --port is required to flash. Pass --no-flash to build only.',
              file=sys.stderr)
        return 1

    try:
        build_and_flash(args.port, args.target, args.board,
                        app, userdata, tone, do_flash)
    except subprocess.CalledProcessError as e:
        return e.returncode or 1

    if not do_flash or args.no_monitor:
        return 0

    rc = monitor_until_result(args.port, args.monitor_timeout)
    if rc == 0:
        print('[result] sdcard_writer SUCCESS - firmware staged on SD card.')
    elif rc == 1:
        print('[result] sdcard_writer FAILED - check serial log above.',
              file=sys.stderr)
    else:
        print('[result] timeout waiting for SUCCESS/FAILED marker.',
              file=sys.stderr)
    return rc


if __name__ == '__main__':
    raise SystemExit(main())
