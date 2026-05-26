# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32', 'esp32s3'], indirect=['target'])
def test_esp_wifi_service_ut(dut: IdfDut) -> None:
    dut.expect(r'\d+ Tests 0 Failures 0 Ignored', timeout=600)
    dut.expect(r'ESP_WIFI_SERVICE_UT_DONE', timeout=30)
