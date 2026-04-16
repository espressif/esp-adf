# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
import os

from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32c2
@pytest.mark.esp32c3
@pytest.mark.esp32c5
@pytest.mark.esp32c6
@pytest.mark.esp32s2
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.esp32h4
def test_esp_audio_analyzer_app(dut: Dut)-> None:
    dut.expect(r'Initializing board...', timeout=10)
    dut.expect(r'Initializing audio system...', timeout=10)
    dut.expect(r'Connecting to WiFi...', timeout=30)
    dut.expect(r'Starting WebSocket server...', timeout=30)
    dut.expect(r'ESP Audio Analyzer Started!', timeout=30)
