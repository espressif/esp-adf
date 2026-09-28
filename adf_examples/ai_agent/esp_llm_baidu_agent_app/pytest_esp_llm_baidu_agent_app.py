# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_llm_baidu_agent_app(dut: Dut) -> None:
    dut.expect(r"APP_MAIN: Start 'network': Wi-Fi connected", timeout=60)
    dut.expect(r'APP_MAIN: Baidu agent is ready', timeout=120)
