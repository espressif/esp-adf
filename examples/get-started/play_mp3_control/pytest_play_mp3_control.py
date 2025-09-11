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
@pytest.mark.esp32p4
@pytest.mark.ADF_EXAMPLE_GENERIC
def test_play_mp3_control(dut: Dut)-> None:
    dut.expect(r'Receive music info from', timeout=100)
