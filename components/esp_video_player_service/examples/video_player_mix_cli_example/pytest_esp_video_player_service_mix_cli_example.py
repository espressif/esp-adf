# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_esp_video_player_service_mix_cli_example(dut: IdfDut) -> None:
    dut.expect(r"Start 'video_player_mix_cli_example'", timeout=60)
    dut.expect(r'CLI ready', timeout=60)

    dut.write('list\n')
    dut.expect(r'test\.mp4', timeout=30)

    dut.write('start movie\n')
    dut.expect(r'Movie started|start movie failed', timeout=60)

    dut.write('play 0\n')
    dut.expect(r'Playing index 0|play 0 failed|es mode', timeout=60)

    dut.write('next\n')
    dut.expect(r'Next:|next failed|es mode', timeout=60)

    dut.write('start tts\n')
    dut.expect(r'TTS started|start tts failed', timeout=60)

    dut.write('status\n')
    dut.expect(r'BACKGROUND', timeout=30)

    dut.write('stop tts\n')
    dut.expect(r'TTS stopped', timeout=60)

    dut.write('stop movie\n')
    dut.expect(r'Movie stopped', timeout=60)

    dut.write('start es\n')
    dut.expect(r'Feeding started|ES started|start es failed', timeout=60)

    dut.write('stop es\n')
    dut.expect(r'ES stopped|Feeding stopped', timeout=60)

    dut.write('start link\n')
    dut.expect(r'LINK started|start link failed', timeout=60)

    dut.write('stop link\n')
    dut.expect(r'LINK stopped', timeout=60)
