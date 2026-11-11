# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut


@pytest.mark.parametrize('target', ['esp32', 'esp32s3'], indirect=True)
def test_bt_hub_console(dut: Dut) -> None:
    # The console prints this line only after the hub reaches RUNNING and the
    # REPL is live; the radio-dependent console flows stay manual.
    dut.expect(r"Start 'bt_hub' console: success", timeout=60)
