# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_rtsp_push(dut: Dut) -> None:
    dut.expect(r'=== RTSP Push Example ===', timeout=15)
    dut.expect(r'Wi-Fi connected', timeout=60)
    dut.expect(r'Pushing camera and microphone to rtsp://', timeout=30)
    dut.expect(r'RTSP push stopped', timeout=90)
    dut.expect(r'Pushing camera and microphone to rtsp://', timeout=30)
    dut.expect(r'RTSP push lifecycle completed', timeout=90)
