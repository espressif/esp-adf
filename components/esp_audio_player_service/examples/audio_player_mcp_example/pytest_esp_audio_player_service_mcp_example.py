# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_audio_player_service_mcp_example(dut: IdfDut) -> None:
    dut.expect(r"Start 'audio_player_mcp_example'", timeout=60)
    dut.expect(r'Audio player MCP tools registered with service manager', timeout=120)
    dut.expect(r'MCP HTTP transport started: http://', timeout=120)
    dut.expect(r'Ready for MCP tools/list and tools/call over HTTP', timeout=30)
