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
@pytest.mark.esp32s2
@pytest.mark.esp32s3
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_cli(dut: Dut)-> None:
    dut.expect(r'ESP_AUDIO_CTRL', timeout=5)

    dut.write('help')
    dut.expect(r'Get freertos all task states information')

    if (os.environ.get('AUDIO_BOARD') != 'ESP32_S2_KALUGA_1_V1_2'):
      print("AUDIO_BOARD:")
      print(os.environ.get('AUDIO_BOARD'))
      dut.write('scan /sdcard')
      dut.expect(r'ID   URL')

      dut.write('play 0')
      dut.expect(r'End of time:', timeout=100)

    dut.write('join Audio_CI esp123456')
    dut.expect(r'Got ip:', timeout=100)

    dut.write('wifi')
    dut.expect(r'Connected Wi-Fi, SSID:')

    dut.write('play https://audiofile.espressif.cn/103_44100_2_320138_188.mp3')
    dut.expect(r'ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0', timeout=100)

    dut.write('pause')
    dut.expect(r'esp_auido status:2,err:0')

    dut.write('resume')
    dut.expect(r'esp_auido status:1,err:0')
