# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32s31', 'esp32p4'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_esp_llm_baidu(dut: IdfDut) -> None:
    dut.expect(r'esp_llm_baidu tests complete', timeout=120)
