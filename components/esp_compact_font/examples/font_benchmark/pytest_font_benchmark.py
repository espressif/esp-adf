# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_font_benchmark(dut: IdfDut) -> None:
    """Accept either a full COMPARE line or the missing-font warning when
    atlases were not prepared under examples/font_benchmark/fonts/."""
    dut.expect(
        r'(COMPARE with_ram=|BENCH bit=|BENCH bit=\d+ codec=(lvgl|gfh) missing)',
        timeout=180,
    )
