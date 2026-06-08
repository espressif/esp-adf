# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32p4'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_music_player(dut: IdfDut) -> None:
    dut.expect(r'\[ 1 \] Initialize board peripherals', timeout=120)
    dut.expect(r'SD card mounted at', timeout=120)
    dut.expect(r'\[ 2 \] Initialize display and LVGL music UI', timeout=120)
    dut.expect(r'\[ 3 \] Scan SD card playlist from', timeout=120)
    dut.expect(
        r'(Scanned [1-9][0-9]* tracks under|No music found, place mp3/aac/wav files in)',
        timeout=120,
    )
    dut.expect(r'\[ 4 \] Start playback controller', timeout=120)
    dut.expect(r'\[ 5 \] Music player ready', timeout=120)
