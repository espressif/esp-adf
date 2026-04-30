# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_services_hub(dut: IdfDut) -> None:
    dut.expect(r'PROD_REAL_CASE', timeout=60)
    dut.expect(r'Real case flow started', timeout=300)
