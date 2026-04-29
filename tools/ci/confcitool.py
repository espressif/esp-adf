# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
"""Configuration module for CI tools.

It mainly defines path defaults for ``apps.yaml`` and ``ci-config.yml``.
Other GitLab CI settings are defined in ``ci-config.yml``.
"""

from __future__ import annotations

import os
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths relative to project root (defaults)
# ---------------------------------------------------------------------------
DEFAULT_APPS_YAML_RELPATH = 'tools/ci/apps.yaml'
DEFAULT_CI_CONFIG_RELPATH = '.gitlab/ci/ci-config.yml'

_ENV_APPS_YAML_RELPATH = 'CI_APPS_YAML_RELPATH'
_ENV_CI_CONFIG_RELPATH = 'CI_CI_CONFIG_RELPATH'

# ---------------------------------------------------------------------------
# CLI defaults for ci-tools
# (not the single source of truth; only to avoid scattered hard-coded strings)
# ---------------------------------------------------------------------------
DEFAULT_SCANNER_IDF_VER = 'v5.3'
DEFAULT_SCANNER_BOARD = 'ESP32_S3_KORVO2_V3'


def rel_apps_yaml() -> str:
    """Return ``apps.yaml`` path relative to project root (POSIX, no leading/trailing ``/``)."""
    return os.environ.get(_ENV_APPS_YAML_RELPATH, DEFAULT_APPS_YAML_RELPATH).strip().strip('/')


def rel_ci_config_yml() -> str:
    """Return ``ci-config.yml`` path relative to project root."""
    return os.environ.get(_ENV_CI_CONFIG_RELPATH, DEFAULT_CI_CONFIG_RELPATH).strip().strip('/')


def path_apps_yaml_under(project_root: str | Path) -> Path:
    return Path(project_root) / rel_apps_yaml()


def path_ci_config_under(project_root: str | Path) -> Path:
    return Path(project_root) / rel_ci_config_yml()
