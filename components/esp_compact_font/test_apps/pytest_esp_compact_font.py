# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_compact_font_ut(dut: IdfDut) -> None:
    dut.expect(r'Running esp_compact_font unit tests', timeout=60)
    dut.expect(r'\d+ Tests 0 Failures 0 Ignored', timeout=800)
    dut.expect(r'ESP_COMPACT_FONT_UT_DONE', timeout=200)
