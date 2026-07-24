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
import time
import pytest
from pytest_embedded import Dut

@pytest.mark.esp32
@pytest.mark.esp32s2
@pytest.mark.esp32s3
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
@pytest.mark.flaky(reruns=1, reruns_delay=5)
def test_cli(dut: Dut)-> None:
    # Wait until app_main finishes: console and sdcard playlist are created after
    # ESP_AUDIO_CTRL, so matching ESP_AUDIO_CTRL alone races with help/scan.
    dut.expect(r'Returned from app_main', timeout=30)

    dut.write('help')
    dut.expect(r'Get freertos all task states information')

    if (os.environ.get('AUDIO_BOARD') != 'ESP32_S2_KALUGA_1_V1_2'):
        print('AUDIO_BOARD:')
        print(os.environ.get('AUDIO_BOARD'))
        dut.write('scan /sdcard')
        dut.expect(r'ID   URL')

        # Only check that playback starts; waiting for End of time depends on
        # whichever file is index 0 and often exceeds the old 100s timeout.
        dut.write('play 0')
        dut.expect(r'ESP_AUDIO status is AEL_STATUS_STATE_RUNNING', timeout=30)

    dut.write('join Audio_CI esp123456')
    dut.expect(r'Got ip:', timeout=100)

    dut.write('wifi')
    dut.expect(r'Connected Wi-Fi, SSID:')

    dut.write('play https://audiofile.espressif.cn/103_44100_2_320138_188.mp3')
    dut.expect(r'ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0', timeout=100)

    time.sleep(2)
    dut.write('pause')
    dut.expect(r'esp_auido status:2,err:0')

    time.sleep(2)
    dut.write('resume')
    dut.expect(r'esp_auido status:1,err:0')
