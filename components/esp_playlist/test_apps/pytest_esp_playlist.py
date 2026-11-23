# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import time

import pexpect
import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize

MENU_READY = 'Press ENTER to see the list of tests'
MENU_HEADER = "Here's the test menu, pick your combo:"
MENU_FOOTER = 'Enter test for running.'


@pytest.fixture
def config(request: pytest.FixtureRequest) -> str:
    return request.param


@pytest.mark.generic
@pytest.mark.timeout(1800)
@pytest.mark.parametrize('config', ['default', 'lfn_none'], indirect=True)
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_playlist(dut: Dut) -> None:
    # Large menus over USB-Serial/JTAG can drop the tail of the first burst after reset,
    # causing confirm_write(..., timeout=1) to resend Enter and match across two menus.
    dut.expect_exact(MENU_READY)
    time.sleep(0.2)
    for _ in range(5):
        dut.write('')
        try:
            dut.expect_exact(MENU_HEADER, timeout=3)
            raw_menu = dut.expect_exact(MENU_FOOTER, return_what_before_match=True, timeout=5)
            body = raw_menu.decode('utf-8').rsplit(MENU_HEADER, 1)[-1]
            menu = dut._parse_unity_menu_from_str(body)
        except (pexpect.TIMEOUT, UnicodeDecodeError, NotImplementedError):
            time.sleep(0.2)
            continue
        if menu:
            dut._test_menu = menu
            dut._hard_reset()
            break
    else:
        raise RuntimeError('Failed to parse esp_playlist unity test menu')

    dut.run_all_single_board_cases(timeout=600)
