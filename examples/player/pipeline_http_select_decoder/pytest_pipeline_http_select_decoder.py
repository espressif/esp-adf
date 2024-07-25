# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s2
@pytest.mark.esp32s3
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_cli(dut: Dut)-> None:
    dut.expect(r'Receive music info from', timeout=20)
