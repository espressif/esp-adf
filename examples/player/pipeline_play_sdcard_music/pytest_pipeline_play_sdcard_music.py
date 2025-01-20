# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import pytest
import os

from pytest_embedded import Dut
from audio_test_package.audio_recorder.recorder_stream import Recorder

@pytest.mark.esp32
@pytest.mark.AUDIO_LOOPBACK_ENV
def test_audio_similarity_compare(dut: Dut)-> None:
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    filename = os.path.join(script_dir, 'output.wav')
    pipeline_id = os.getenv('CI_PIPELINE_ID')
    job_id = os.getenv('CI_JOB_ID')
    media_ci_server = os.getenv('MEDIA_CI_SERVER')

    recorder = Recorder(filename=filename, device='plughw:G3,0', channels=1, sample_rate=44100)
    dut.expect(r'Receive music info from', timeout=20)
    # Start the Runner recording thread
    recorder.start()

    dut.expect('Stop event received')
    # Stop the Runner recording thread
    recorder.stop()

    # Upload audio to the server to calculate audio similarity
    url = f"{media_ci_server}/upload/?pipeline_id={pipeline_id}&job_id={job_id}&target_file=test.wav"
    recorder.request_audio_similarity(url)

@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_str_detect(dut: Dut)-> None:
    dut.expect(r'Receive music info from', timeout=20)
