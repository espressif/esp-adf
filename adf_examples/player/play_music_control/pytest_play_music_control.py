# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32', 'esp32s3', 'esp32p4'], indirect=['target'])
def test_play_music_control(dut: Dut) -> None:
    dut.expect(r'=== Play Music Control', timeout=15)
    dut.expect(r'\[ 5 \] Connect to WiFi', timeout=60)
    dut.expect(r'WiFi connected', timeout=60)
    dut.expect(r'CLI ready', timeout=15)

    dut.write('help')
    dut.expect(r'Show current player status')

    dut.write('list')
    dut.expect(r'Music File List')

    dut.write('play 1')
    dut.expect(r'Song finished', timeout=100)
