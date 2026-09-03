# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_audio_player_service_mix_cli_example(dut: IdfDut) -> None:
    dut.expect(r"Start 'audio_player_mix_cli_example'", timeout=60)
    dut.expect(r'CLI ready', timeout=60)

    dut.write('list')
    dut.expect(r'test\.mp3', timeout=30)

    dut.write('start url')
    dut.expect(r'URL started', timeout=60)

    dut.write('start feed')
    dut.expect(r'FEED started', timeout=60)

    dut.write('status')
    dut.expect(r'BACKGROUND', timeout=30)

    dut.write('stop feed')
    dut.expect(r'FEED stopped', timeout=60)

    dut.write('stop url')
    dut.expect(r'URL stopped', timeout=60)

    dut.write('start link')
    dut.expect(r'LINK started|start link failed', timeout=60)

    dut.write('stop link')
    dut.expect(r'LINK stopped', timeout=60)
