'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py examples/usb/device/usb_uf2_nvs -t esp32s2
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest examples/usb/device/usb_uf2_nvs --target esp32s2
'''

import os
import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s3
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_audio_mixer_tone(dut: Dut)-> None:
    dut.expect(r'Returned from app_main', timeout=100)

    dut.write('help')
    dut.expect(r'Get freertos all task states information')

    dut.write('join Audio_CI esp123456')
    dut.expect(r'Got ip:', timeout=100)

    dut.write('play file://sdcard/test.mp3#raw 1')
    dut.expect(r'ESP_AUDIO status is AEL_STATUS_STATE_RUNNING')

    dut.write('pmixer https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3 3')
    dut.expect(r'Received music info from', timeout=100)

    dut.write('gmixer 1 -3')
    dut.expect(r'Reset the audio mixer')

    dut.write('smixer 3')
    dut.expect(r'Mixer manager REC CMD:3')

    dut.write('stop')
    dut.expect(r'Exit media_ctrl_stop procedure, ret:0')

    dut.write('record 1')
    dut.expect(r'Start record', timeout=100)

    dut.write('record 0')
    dut.expect(r'Stop record', timeout=100)
