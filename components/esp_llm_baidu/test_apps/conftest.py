# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: Apache-2.0

import os
import subprocess
from pathlib import Path

os.environ.setdefault('ESP_BMGR_ASSIST_PREFLIGHT', '0')

_PROJECT_DIR = Path(__file__).resolve().parent
_BOARD_FOR_TARGET = {
    'esp32s3': 'esp32_s3_korvo_2_3',
    'esp32s31': 'esp32_s31_korvo_1',
    'esp32p4': 'esp32_p4_function_ev_board',
}


def _first_str(value):
    if value is None:
        return None
    if isinstance(value, (list, tuple)):
        return value[0] if value else None
    return str(value)


def pytest_configure(config):
    target = _first_str(getattr(config.option, 'target', None))
    board = _BOARD_FOR_TARGET.get(target)
    gen_dir = _PROJECT_DIR / 'components' / 'gen_bmgr_codes'
    if board is None or gen_dir.exists():
        return
    subprocess.check_call(['idf.py', 'bmgr', '-b', board], cwd=_PROJECT_DIR)
