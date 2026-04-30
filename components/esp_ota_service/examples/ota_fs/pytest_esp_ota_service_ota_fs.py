# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3'], indirect=['target'])
def test_esp_ota_service_ota_fs_example(dut: IdfDut) -> None:
    dut.expect(r'OTA_FS_APP', timeout=30)
    dut.expect(r'Running version', timeout=180)
