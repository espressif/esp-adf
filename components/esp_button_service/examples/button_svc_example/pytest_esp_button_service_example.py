# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_esp_button_service_example(dut: IdfDut) -> None:
    dut.expect(r'esp_button_service example', timeout=60)
    dut.expect(r'press any key to see events', timeout=60)
