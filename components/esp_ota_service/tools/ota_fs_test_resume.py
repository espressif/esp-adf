#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# Hardware-in-the-loop test for the ota_fs resume-from-offset path.
#
# Flow
# ----
#   Stage A  build ota_fs @ --new-version and publish it as
#            firmware_samples/ota_fs_v<new>.bin (+ userdata) via build_firmware.py.
#   Stage B  stage the new bin onto the SD card with ota_fs_flash_sdcard.py,
#            waiting for the writer's SUCCESS marker.
#   Stage C  rebuild + flash ota_fs at the original (older) version so the
#            running firmware will pick up the newer bin from SD on boot.
#   Stage D  monitor serial: when OTA progress crosses --min-written, issue a
#            USB hard-reset. On reboot, expect OTA_SERVICE to log
#            "Resuming from offset <N>" (NVS resume path).
#
# The heavy lifting is delegated to the new wrapper scripts:
#   build_firmware.py       - idf.py build + publish versioned bin
#   ota_fs_flash_sdcard.py  - sdcard_writer build + flash + monitor
#
# Typical use
# -----------
#   ./ota_fs_test_resume.py --port /dev/ttyUSB0 --new-version 0.2.0
#   ./ota_fs_test_resume.py --port /dev/ttyUSB0 --skip-sd-stage  # reuse existing SD
#
# Requires: pip install pyserial

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

from _fw_common import (
    ADF_ROOT,
    TOOLS_DIR,
    example_dir,
    parse_semver,
)

try:
    import serial
except ImportError:
    print('Install pyserial: pip install pyserial', file=sys.stderr)
    raise

PROGRESS_RE = re.compile(rb'\[ota\] progress:\s+(\d+)\s+/\s+(\d+)')
RESUME_RE = re.compile(rb'Resuming from offset\s+(\d+)')


def _run(cmd: list[str], cwd: Path | None = None) -> None:
    print(f'+ (cwd {cwd or os.getcwd()}) {' '.join(cmd)}', flush=True)
    subprocess.check_call(cmd, cwd=str(cwd) if cwd else None)


def usb_serial_hard_reset(ser: serial.Serial) -> None:
    """Typical USB-UART wiring: RTS -> EN, DTR -> GPIO0."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.12)
    ser.rts = False
    time.sleep(0.15)


def wait_pattern(ser: serial.Serial, pattern: re.Pattern[bytes],
                 overall_s: float) -> re.Match[bytes] | None:
    buf = b''
    deadline = time.monotonic() + overall_s
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
            buf += chunk
            if len(buf) > 256 * 1024:
                buf = buf[-128 * 1024:]
            m = pattern.search(buf)
            if m:
                return m
        else:
            time.sleep(0.01)
    return pattern.search(buf)


def wait_for_progress(ser: serial.Serial, min_written: int,
                      overall_s: float) -> int | None:
    buf = b''
    deadline = time.monotonic() + overall_s
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
            buf += chunk
            if len(buf) > 256 * 1024:
                buf = buf[-128 * 1024:]
            for m in PROGRESS_RE.finditer(buf):
                written = int(m.group(1))
                if written >= min_written:
                    return written
        else:
            time.sleep(0.01)
    return None


class _PortGrabber(threading.Thread):
    """Retry opening the serial port until esptool releases it (post-flash)."""

    def __init__(self, port: str) -> None:
        super().__init__(daemon=True)
        self._port = port
        self.ser: serial.Serial | None = None
        self._cancel = threading.Event()

    def cancel(self) -> None:
        self._cancel.set()

    def run(self) -> None:
        while self.ser is None and not self._cancel.is_set():
            try:
                self.ser = serial.Serial(self._port, 115200, timeout=0.05)
                return
            except serial.SerialException:
                time.sleep(0.02)


def main() -> int:
    p = argparse.ArgumentParser(
        description='HIL test for the ota_fs resume-from-offset path.',
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--port', default='/dev/ttyUSB0',
                   help='Serial port for the target S3 (default /dev/ttyUSB0).')
    p.add_argument('--new-version', default='0.2.0',
                   help="Semver to publish as the 'new' firmware staged on SD "
                        '(default 0.2.0).')
    p.add_argument('--min-written', type=int, default=131072,
                   help='OTA bytes to wait for before triggering reset '
                        '(default 128 KiB).')
    p.add_argument('--skip-sd-stage', action='store_true',
                   help='Assume SD already has a newer ota_fs_v*.bin staged.')
    p.add_argument('--target', default=None,
                   help='Pass --target to the build steps (e.g. esp32s3).')
    args = p.parse_args()

    try:
        parse_semver(args.new_version)
    except ValueError as e:
        print(f'[error] bad --new-version: {e}', file=sys.stderr)
        return 1

    ex_fs = example_dir('fs')
    version_txt = ex_fs / 'version.txt'
    if not version_txt.is_file():
        print(f'[error] missing {version_txt}; create it with the running version',
              file=sys.stderr)
        return 1
    orig_version = version_txt.read_text().strip()
    try:
        parse_semver(orig_version)
    except ValueError as e:
        print(f'[error] bad original version in {version_txt}: {e}',
              file=sys.stderr)
        return 1

    build_py = TOOLS_DIR / 'build_firmware.py'
    flash_sd_py = TOOLS_DIR / 'ota_fs_flash_sdcard.py'

    try:
        if not args.skip_sd_stage:
            print(f'== Stage A: build ota_fs @ {args.new_version} and publish ==')
            cmd = [sys.executable, str(build_py), 'fs', '-v', args.new_version]
            if args.target:
                cmd += ['--target', args.target]
            _run(cmd)

            print(f'== Stage B: stage firmware onto SD via sdcard_writer ==')
            cmd = [sys.executable, str(flash_sd_py), '--port', args.port]
            if args.target:
                cmd += ['--target', args.target]
            _run(cmd)

            version_txt.write_text(orig_version + '\n')

        print(f'== Stage C: rebuild ota_fs @ {orig_version} and flash S3 ==')
        version_txt.write_text(orig_version + '\n')
        _run([sys.executable, str(build_py), 'fs', '-v', orig_version])
        # Flash ota_fs first, then start the grabber so it only competes for
        # the port after esptool has released it.
        flash_cmd = ['idf.py', '-p', args.port, 'flash']
        _run(flash_cmd, cwd=ex_fs)
        grab = _PortGrabber(args.port)
        grab.start()

        print(f'== Stage D: monitor OTA, reset mid-download, verify resume log ==')
        grab.join(timeout=30.0)
        ser = grab.ser
        if ser is None:
            print('Timeout: could not open serial after flash.', file=sys.stderr)
            grab.cancel()
            grab.join(timeout=2.0)
            return 1

        written = wait_for_progress(ser, args.min_written, overall_s=180.0)
        if written is None:
            print('Timeout: no OTA progress reached threshold. Check SD '
                  'firmware version + board SD init.', file=sys.stderr)
            ser.close()
            return 1
        print(f'\n[hil] progress >= {args.min_written} bytes '
              f'(at ~{written}); issuing USB reset ...', flush=True)
        usb_serial_hard_reset(ser)
        time.sleep(0.3)
        ser.reset_input_buffer()

        m2 = wait_pattern(ser, RESUME_RE, overall_s=120.0)
        ser.close()
        if not m2:
            print("\nFAIL: 'Resuming from offset' not seen after reset.",
                  file=sys.stderr)
            return 2
        off = m2.group(1).decode('ascii', errors='replace')
        print(f'\nPASS: OTA_SERVICE reported resume offset {off}')
        return 0
    except subprocess.CalledProcessError as e:
        return e.returncode or 1


if __name__ == '__main__':
    raise SystemExit(main())
