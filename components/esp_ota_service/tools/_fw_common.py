#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
#
# Shared helpers for the esp_ota_service tools scripts. Keeps path math, semver
# parsing, hashing, and manifest I/O in one place so the per-scenario scripts
# (build_firmware / ota_fs_flash_sdcard / ota_http_serve / ota_ble /
# ota_fs_test_resume) can stay small and focused.

from __future__ import annotations

import hashlib
import json
import re
import socket
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


# ---------------------------------------------------------------------------
# Paths / layout
# ---------------------------------------------------------------------------

TOOLS_DIR = Path(__file__).resolve().parent
OTA_SERVICE_DIR = TOOLS_DIR.parent
EXAMPLES_DIR = OTA_SERVICE_DIR / 'examples'
ADF_ROOT = OTA_SERVICE_DIR.parent.parent
FIRMWARE_SAMPLES_DIR = TOOLS_DIR / 'firmware_samples'
MANIFEST_PATH = FIRMWARE_SAMPLES_DIR / 'manifest.json'

# Supported OTA examples. Key is the short name used on the CLI.
# "project_name" must match project() in the example's CMakeLists.txt.
# Optional "project_dir" absolute Path overrides the default
# EXAMPLES_DIR / dir_name lookup (used for examples hosted outside of
# components/esp_ota_service/examples/, e.g. adf_examples/*).
EXAMPLES: dict[str, dict] = {
    'http': {
        'dir_name': 'ota_http',
        'project_name': 'ota_http',
        'default_url': 'http://0.0.0.0:18070/firmware.bin',
    },
    'fs': {
        'dir_name': 'ota_fs',
        'project_name': 'ota_fs',
        'default_url': 'file:///sdcard/app.bin',
    },
    'ble': {
        'dir_name': 'ota_ble',
        'project_name': 'ota_ble',
        'default_url': 'ble://ota_ble/app.bin',
    },
    'services_hub': {
        'dir_name': 'services_hub',
        'project_name': 'services_hub',
        'default_url': 'http://0.0.0.0:18070/firmware.bin',
        'project_dir': ADF_ROOT / 'adf_examples' / 'services_hub',
    },
}


def example_dir(name: str) -> Path:
    info = EXAMPLES[name]
    override = info.get('project_dir')
    if override is not None:
        return Path(override)
    return EXAMPLES_DIR / info['dir_name']


def versioned_name(project_name: str, version: str, suffix: str = '.bin') -> str:
    """Return canonical versioned artefact name, e.g. 'ota_http_v1.0.0.bin'."""
    return f'{project_name}_v{version}{suffix}'


# ---------------------------------------------------------------------------
# Semver
# ---------------------------------------------------------------------------

_SEMVER_RE = re.compile(r'^v?(\d+)\.(\d+)\.(\d+)$')


def parse_semver(value: str) -> tuple[int, int, int]:
    m = _SEMVER_RE.match(value.strip())
    if not m:
        raise ValueError(f'expected semver x.y.z, got {value!r}')
    maj, mi, pa = (int(g, 10) for g in m.groups())
    for part, label in ((maj, 'major'), (mi, 'minor'), (pa, 'patch')):
        if not 0 <= part <= 255:
            raise ValueError(f'{label} must be 0..255, got {part}')
    return maj, mi, pa


def pack_semver_u32(version: str) -> int:
    """Pack x.y.z into a u32 as (major<<16)|(minor<<8)|patch.

    Matches ota_version_pack_semver() in ota_version_utils.h.  The caller is
    responsible for writing the resulting integer in little-endian byte order
    (e.g. via struct.pack_into('<I', ...)).
    """
    maj, mi, pa = parse_semver(version)
    return (maj << 16) | (mi << 8) | pa


# ---------------------------------------------------------------------------
# Hashing / manifest
# ---------------------------------------------------------------------------

@dataclass
class BinStats:
    size: int
    sha256: str
    md5: str


def hash_file(path: Path) -> BinStats:
    sha = hashlib.sha256()
    md5 = hashlib.md5()
    total = 0
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            sha.update(chunk)
            md5.update(chunk)
            total += len(chunk)
    return BinStats(size=total, sha256=sha.hexdigest(), md5=md5.hexdigest())


def load_manifest() -> dict:
    if MANIFEST_PATH.is_file():
        try:
            return json.loads(MANIFEST_PATH.read_text())
        except json.JSONDecodeError:
            return {}
    return {}


def save_manifest(manifest: dict) -> None:
    FIRMWARE_SAMPLES_DIR.mkdir(parents=True, exist_ok=True)
    manifest = dict(manifest)
    manifest['updated_at'] = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=4, sort_keys=True) + '\n')


# ---------------------------------------------------------------------------
# firmware_samples lookup
# ---------------------------------------------------------------------------

_VER_SUFFIX_RE = re.compile(r'_v(\d+)\.(\d+)\.(\d+)$')


def find_versioned_bins(stem_prefix: str) -> list[Path]:
    """Return all files in firmware_samples matching '<stem_prefix>_v<semver>.bin',
    sorted from newest semver to oldest."""
    if not FIRMWARE_SAMPLES_DIR.is_dir():
        return []
    candidates: list[tuple[tuple[int, int, int], Path]] = []
    for p in FIRMWARE_SAMPLES_DIR.glob(f'{stem_prefix}_v*.bin'):
        m = _VER_SUFFIX_RE.search(p.stem)
        if not m:
            continue
        candidates.append(((int(m.group(1)), int(m.group(2)), int(m.group(3))), p))
    candidates.sort(key=lambda x: x[0], reverse=True)
    return [p for _, p in candidates]


def latest_versioned_bin(stem_prefix: str) -> Path | None:
    bins = find_versioned_bins(stem_prefix)
    return bins[0] if bins else None


def resolve_bin(stem_prefix: str, arg: str | None) -> Path:
    """Resolve --bin argument: explicit path / explicit filename inside
    firmware_samples / 'latest' (or None) -> latest versioned bin.

    Raises FileNotFoundError when nothing matches.
    """
    if arg and arg not in ('', 'latest'):
        p = Path(arg)
        if not p.is_absolute():
            alt = FIRMWARE_SAMPLES_DIR / arg
            if alt.is_file():
                p = alt
        if not p.is_file():
            raise FileNotFoundError(f'bin not found: {arg}')
        return p
    latest = latest_versioned_bin(stem_prefix)
    if latest is None:
        raise FileNotFoundError(
            f'no {stem_prefix}_v*.bin in {FIRMWARE_SAMPLES_DIR}. '
            f'Run: build_firmware.py <example> first.'
        )
    return latest


# ---------------------------------------------------------------------------
# Host helpers
# ---------------------------------------------------------------------------

def detect_host_ip() -> str:
    """Best-effort non-loopback IPv4 address; falls back to 0.0.0.0."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(('8.8.8.8', 53))
            return s.getsockname()[0]
    except OSError:
        return '0.0.0.0'
