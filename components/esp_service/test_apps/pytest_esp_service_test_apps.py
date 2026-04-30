# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_esp_service_test_apps(dut: IdfDut) -> None:
    dut.expect(r'ESP Service - test_apps', timeout=60)
    dut.expect(r'TEST_APPS RUN COMPLETE', timeout=600)
