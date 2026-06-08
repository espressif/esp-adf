# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest

from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_av_record_live_display(dut: Dut) -> None:
    dut.expect(r'Display fps=', timeout=60)
    dut.expect(r'Display loop finished, frames=[1-9][0-9]*, bad_frames=0', timeout=30)
    dut.expect(r'Record file size: .+ -> [1-9][0-9]* bytes', timeout=30)
    dut.expect(r'Example finished', timeout=60)
    dut.expect(r'All resources released', timeout=30)
