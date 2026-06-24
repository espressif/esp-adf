# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_esp_media_service(dut: IdfDut) -> None:
    dut.expect(r'esp_media_service tests complete', timeout=600)
