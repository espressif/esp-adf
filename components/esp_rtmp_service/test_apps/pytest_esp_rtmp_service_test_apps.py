# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_esp_rtmp_service_ut(dut: IdfDut) -> None:
    dut.expect(r'Running esp_rtmp_service unit tests', timeout=60)
    dut.expect(r'\d+ Tests 0 Failures 0 Ignored', timeout=600)
    dut.expect(r'ESP_RTMP_SERVICE_UT_DONE', timeout=30)
