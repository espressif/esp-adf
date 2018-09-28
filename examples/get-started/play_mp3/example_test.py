import os
import sys

# this is a test case written with tiny-test-fw.
# to run test cases outside tiny-test-fw,
# we need to set environment variable `TEST_FW_PATH`,
# then get and insert `TEST_FW_PATH` to sys path before import FW module

test_fw_path = os.getenv("TEST_FW_PATH")
if test_fw_path and test_fw_path not in sys.path:
    sys.path.insert(0, test_fw_path)

auto_test_path = os.getenv("AUTO_TEST_PATH")
if auto_test_path and auto_test_path not in sys.path:
    sys.path.insert(0, auto_test_path)

import TinyFW
import NormalProject
from NormalProject.ProjectDUT import ProDUT
from NormalProject.ProjectApp import Example
from BasicUtility.RecordAudioFile import AudioRecord
import ADFExampleTest


@NormalProject.example_test(env_tag="Example_AUDIO_PLAY", ignore=True)
@ADFExampleTest.play_test(os.path.join(os.getenv("ADF_PATH"), "examples/get-started/play_mp3/main/adf_music.mp3"),
os.path.join(os.getenv("ADF_PATH"), "examples/get-started/play_mp3/main/dest.wav"))
def example_test_play_mp3(env, extra_data):
    dut1 = env.get_dut("play_mp3", "examples/get-started/play_mp3", pro_path=os.getenv("ADF_PATH"))

    # start test
    dut1.start_app()
    dut1.reset()
    dut1.expect("[ 1 ] Start audio codec chip", timeout=30)
    dut1.expect("[ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event")
    dut1.expect("[2.1] Create mp3 decoder to decode mp3 file and set custom read callback")
    dut1.expect("[2.2] Create i2s stream to write data to codec chip")
    dut1.expect("[2.3] Register all elements to audio pipeline")
    dut1.expect("[2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]")
    dut1.expect("[ 3 ] Setup event listener")
    dut1.expect("[3.1] Listening event from all elements of pipeline")
    dut1.expect("[ 4 ] Start audio_pipeline")
    dut1.expect("[ 5 ] Stop audio_pipeline", timeout=30)



if __name__ == '__main__':
    example_test_play_mp3()
