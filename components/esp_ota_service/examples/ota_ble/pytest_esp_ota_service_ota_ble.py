# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_esp_ota_service_ota_ble_example(dut: IdfDut) -> None:
    # Do not wait for app_main "Idle" — run_ota can block a long time without a BLE central.
    dut.expect(r'BLE OTA', timeout=60)
    dut.expect(r'Run_ota: BLE OTA \(APP protocol\) ready', timeout=240)
