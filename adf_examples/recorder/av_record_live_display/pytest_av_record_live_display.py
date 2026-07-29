# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest

from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32s31', 'esp32p4'], indirect=['target'])
def test_av_record_live_display(dut: Dut) -> None:
    dut.expect(r'\[ 4 \] Start capture and interactive live display', timeout=60)
    dut.expect(r'Interactive UI ready, touch=(yes|no), offset=\([0-9]+,[0-9]+\)', timeout=30)
    dut.expect(r'Live display loop started', timeout=30)
    dut.expect(r'Display fps=', timeout=60)
