# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_esp_service_mock_services_example(dut: IdfDut) -> None:
    dut.expect(r'Mock Services Example', timeout=60)
    dut.expect(r'ALL SMOKE TESTS PASSED', timeout=120)
