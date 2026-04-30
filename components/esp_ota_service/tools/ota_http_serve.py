#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# HTTP OTA server shared by the ota_http and services_hub examples.
#
# Picks a versioned payload out of tools/firmware_samples/ (default: newest
# <project>_v*.bin, where <project> is derived from --example) and serves it
# at two stable paths:
#
#     GET /firmware.bin        -> the selected bin (what the device URL points at)
#     GET /<filename>          -> the same bin, under its versioned name
#     GET /manifest.json       -> firmware_samples/manifest.json (if present)
#
# Other requests return 404 so random scanners don't leak the firmware tree.
#
# Usage
# -----
#   # Serve the newest ota_http_v*.bin on :18070 (default example: http)
#   ./ota_http_serve.py
#
#   # Serve the newest services_hub_v*.bin
#   ./ota_http_serve.py --example services_hub
#
#   # Pin to a specific version
#   ./ota_http_serve.py --bin services_hub_v1.0.1.bin --port 18070
#
#   # Rewrite manifest.json's url to advertise the host IP on which we're serving
#   ./ota_http_serve.py --example services_hub --update-manifest-url

from __future__ import annotations

import argparse
import http.server
import socketserver
import sys
from pathlib import Path

from _fw_common import (
    EXAMPLES,
    FIRMWARE_SAMPLES_DIR,
    MANIFEST_PATH,
    detect_host_ip,
    load_manifest,
    resolve_bin,
    save_manifest,
)

def make_handler(bin_path: Path):
    bin_name = bin_path.name
    bin_bytes = bin_path.read_bytes()

    manifest_bytes: bytes | None = None
    if MANIFEST_PATH.is_file():
        manifest_bytes = MANIFEST_PATH.read_bytes()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args) -> None:
            sys.stdout.write(f'[http] {self.address_string()} - {fmt % args}\n')
            sys.stdout.flush()

        def _send_bytes(self, data: bytes, content_type: str) -> None:
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_HEAD(self) -> None:
            self._handle(write_body=False)

        def do_GET(self) -> None:
            self._handle(write_body=True)

        def _handle(self, *, write_body: bool) -> None:
            path = self.path.split('?', 1)[0]
            if path in ('/firmware.bin', f'/{bin_name}'):
                if write_body:
                    self._send_bytes(bin_bytes, 'application/octet-stream')
                else:
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/octet-stream')
                    self.send_header('Content-Length', str(len(bin_bytes)))
                    self.send_header('Cache-Control', 'no-store')
                    self.end_headers()
                return

            if path == '/manifest.json' and manifest_bytes is not None:
                if write_body:
                    self._send_bytes(manifest_bytes, 'application/json')
                else:
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Content-Length', str(len(manifest_bytes)))
                    self.end_headers()
                return

            self.send_error(404, 'not found')

    return Handler


class ReuseAddrServer(socketserver.TCPServer):
    allow_reuse_address = True


def _maybe_update_manifest_url(bin_path: Path, host_ip: str, port: int) -> None:
    manifest = load_manifest()
    if not manifest:
        return
    if manifest.get('file') != bin_path.name:
        print(f'[warn] manifest.json is tracking {manifest.get('file')!r}, '
              f'not {bin_path.name!r}. Run build_firmware.py http first if '
              f'you want them aligned.')
    manifest['url'] = f'http://{host_ip}:{port}/firmware.bin'
    save_manifest(manifest)
    print(f'[manifest] url -> {manifest['url']}')


def main() -> int:
    p = argparse.ArgumentParser(
        description='Serve a versioned OTA firmware over HTTP for the '
                    'ota_http / services_hub examples.')
    p.add_argument('-e', '--example', default='http', choices=list(EXAMPLES),
                   help='Example short name from EXAMPLES (default: http). '
                        'Determines the <project>_v*.bin lookup pattern.')
    p.add_argument('--bin', default=None,
                   help='Path or filename inside firmware_samples/. '
                        'Default: newest <project>_v*.bin matching --example.')
    p.add_argument('--port', type=int, default=18070,
                   help='Listening port (default 18070).')
    p.add_argument('--host', default='0.0.0.0',
                   help='Bind address (default 0.0.0.0).')
    p.add_argument('--update-manifest-url', action='store_true',
                   help='Rewrite firmware_samples/manifest.json url to point '
                        'at this host:port before serving.')
    args = p.parse_args()

    stem_prefix = EXAMPLES[args.example]['project_name']
    try:
        bin_path = resolve_bin(stem_prefix, args.bin)
    except FileNotFoundError as e:
        print(f'[error] {e}', file=sys.stderr)
        return 1

    ip = detect_host_ip()

    if args.update_manifest_url:
        _maybe_update_manifest_url(bin_path, ip, args.port)

    size_kib = bin_path.stat().st_size / 1024.0
    print('=' * 60)
    print(f'  Serving {bin_path.name}  ({size_kib:.1f} KiB)')
    print(f'  From    {FIRMWARE_SAMPLES_DIR}')
    print(f'  Bind    http://{args.host}:{args.port}')
    print(f'  URL     http://{ip}:{args.port}/firmware.bin')
    if MANIFEST_PATH.is_file():
        print(f'  Manifest http://{ip}:{args.port}/manifest.json')
    print('=' * 60)
    print('  Set OTA_SERVER_URL in examples/ota_http/main/ota_http_example.c')
    print(f'  to the URL above, then idf.py build flash monitor.')
    print('  Press Ctrl+C to stop.')
    print()

    handler = make_handler(bin_path)
    with ReuseAddrServer((args.host, args.port), handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print('\n[http] stopped', flush=True)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
