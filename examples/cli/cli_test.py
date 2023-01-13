#!/usr/bin/env python

from __future__ import division, print_function, unicode_literals

import ttfw_idf

@ttfw_idf.idf_example_test(env_tag='ADF_EXAMPLE_GENERIC', target=['esp32', 'esp32s2', 'esp32s3'], ci_target=['esp32', 'esp32s3'])
def test_examples_system(env, extra_data):
    app_name = 'cli'
    dut = env.get_dut(app_name, 'examples/cli')
    dut.start_app()
    dut.expect('esp_audio instance is')

    dut.write('help')
    dut.expect('Get freertos all task states information')

    dut.write('scan /sdcard')
    dut.expect('ID   URL')

    dut.write('play 0')
    dut.expect('[ * ] End of time:')

    dut.write('join Audio_CI esp123456')
    dut.expect('Got ip:')

    dut.write('wifi')
    dut.expect('Connected Wi-Fi, SSID:')

    dut.write('play https://audiofile.espressif.cn/103_44100_2_320138_188.mp3')
    dut.expect('ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0')

    dut.write('pause')
    dut.expect('esp_auido status:2,err:0')

    dut.write('resume')
    dut.expect('esp_auido status:1,err:0')

if __name__ == '__main__':
    test_examples_system()
