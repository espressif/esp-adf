# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_rtsp_cli(dut: Dut) -> None:
    dut.expect(r'=== RTSP Example ===', timeout=15)
    dut.expect(r'\[ 4 \] Connect to WiFi', timeout=60)
    dut.expect(r'\[ 5 \] Start CLI', timeout=60)
    dut.expect(r'CLI ready', timeout=15)

    dut.write('help')
    dut.expect(r'RTSP control')

    dut.write('rtsp info')
    dut.expect(r'session\s+: idle')

    # The server role needs no remote peer, so it can be started and stopped on its own
    dut.write('rtsp server')
    dut.expect(r'server session running on rtsp://0\.0\.0\.0:554/live', timeout=30)

    dut.write('rtsp info')
    dut.expect(r'session\s+: server')

    dut.write('rtsp stop')
    dut.expect(r'Stopping the server session', timeout=30)

    dut.write('rtsp info')
    dut.expect(r'session\s+: idle')
