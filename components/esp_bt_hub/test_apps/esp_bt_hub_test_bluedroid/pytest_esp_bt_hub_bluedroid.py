# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.parametrize('target', ['esp32', 'esp32s3'], indirect=True)
@pytest.mark.timeout(8000)
def test_esp_bt_hub_bluedroid(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=600)
