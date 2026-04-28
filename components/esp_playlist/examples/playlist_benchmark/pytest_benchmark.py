# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest

from pytest_embedded import Dut

@pytest.mark.esp32s3
@pytest.mark.esp32p4
def test_playlist_benchmark_detect(dut: Dut)-> None:
    dut.expect(r'PLAYLIST_BENCH: Playlist benchmark done', timeout=30)
