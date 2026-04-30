# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_adf_event_hub_example(dut: IdfDut) -> None:
    dut.expect(r'adf_event_hub', timeout=120)
    dut.expect(r'Simulation Complete', timeout=120)
