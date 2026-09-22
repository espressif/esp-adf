# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_sip_cli(dut: Dut) -> None:
    dut.expect(r'=== SIP Example ===', timeout=15)
    dut.expect(r'\[ 4 \] Connect to WiFi', timeout=60)
    dut.expect(r'\[ 5 \] Start CLI', timeout=60)
    dut.expect(r'CLI ready', timeout=15)

    dut.write('help')
    dut.expect(r'SIP control')

    dut.write('sip info')
    dut.expect(r'session\s+: offline')

    # P2P mode is the only start that needs no registrar, so it is what CI can
    # verify: the chain is built and the local SIP port is open.
    dut.write('sip start --p2p -v none')
    dut.expect(r'SIP session online', timeout=30)

    dut.write('sip info')
    dut.expect(r'session\s+: online')
    dut.expect(r'call\s+: idle')

    # Without a peer there is nothing to answer or hang up, but the commands
    # must still report cleanly instead of faulting
    dut.write('sip answer')
    dut.expect(r'sip answer failed: ESP_ERR_INVALID_STATE')

    dut.write('sip bye')

    dut.write('sip stop')
    dut.expect(r'Stopping the SIP session', timeout=30)

    dut.write('sip info')
    dut.expect(r'session\s+: offline')
