#!/usr/bin/env python

from __future__ import division, print_function, unicode_literals

import ttfw_idf


@ttfw_idf.idf_example_test(env_tag='ADF_EXAMPLE_GENERIC', target=['esp32', 'esp32s2', 'esp32s3', 'esp32c3'], ci_target=['esp32s2'])
def test_examples_player(env, extra_data):
    app_name = 'pipeline_http_mp3'
    dut = env.get_dut(app_name, 'examples/player/pipeline_http_mp3')
    dut.start_app()
    dut.expect('[ 1 ] Start audio codec chip')


if __name__ == '__main__':
    test_examples_player()
