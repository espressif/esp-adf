# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_esp_audio_capture_service_example(dut: IdfDut) -> None:
    dut.expect(r'Audio record example is ready', timeout=120)
    dut.expect(r'AUDIO_RECORD_EXAMPLE_READY', timeout=30)
