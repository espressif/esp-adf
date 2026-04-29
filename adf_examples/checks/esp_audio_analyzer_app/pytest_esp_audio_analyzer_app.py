# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize(
    'target',
    [
        'esp32',
        'esp32c2',
        'esp32c3',
        'esp32c5',
        'esp32c6',
        'esp32s2',
        'esp32s3',
        'esp32p4',
        'esp32h4',
    ],
    indirect=['target']
)
def test_esp_audio_analyzer_app(dut: Dut)-> None:
    dut.expect(r'Initializing board...', timeout=10)
    dut.expect(r'Initializing audio system...', timeout=10)
    dut.expect(r'Connecting to WiFi...', timeout=30)
    dut.expect(r'Starting WebSocket server...', timeout=30)
    dut.expect(r'ESP Audio Analyzer Started!', timeout=30)
