# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.generic
@idf_parametrize('target', ['esp32s3', 'esp32p4'], indirect=['target'])
def test_rtmp_cli(dut: Dut) -> None:
    dut.expect(r'=== RTMP Example ===', timeout=15)
    dut.expect(r'\[ 4 \] Connect to WiFi', timeout=60)
    dut.expect(r'\[ 5 \] Start CLI', timeout=60)
    dut.expect(r'CLI ready', timeout=15)

    dut.write('help')
    dut.expect(r'RTMP control')

    dut.write('rtmp info')
    dut.expect(r'slots\s+: idle')

    dut.write('rtmp loopback -v none')
    dut.expect(r"Local RTMP server listening on port 1935, app 'live'", timeout=30)
    # '-v none' is confirmed here: the publisher reports the track set it announces.
    dut.expect(r'Publishing to rtmp://127\.0\.0\.1:1935/live/stream \(video: none', timeout=30)
    dut.expect(r'Live stream ready: ffplay rtmp://', timeout=30)

    dut.expect(r'Playing rtmp://127\.0\.0\.1:1935/live/stream', timeout=30)

    # Playback only leaves PREPARING once the decoder has had real frames, so this
    # is what proves audio crossed capture, publisher, server and player.
    dut.expect(r'State transition: PREPARING -> PLAYING', timeout=30)

    dut.write('rtmp info')
    dut.expect(r'server\s+: listening on port 1935')
    dut.expect(r'push\s+: rtmp://127\.0\.0\.1:1935/live/stream')
    dut.expect(r'pull\s+: rtmp://127\.0\.0\.1:1935/live/stream')

    dut.write('rtmp stop')
    dut.expect(r'All RTMP slots released', timeout=60)

    dut.write('rtmp info')
    dut.expect(r'slots\s+: idle')
