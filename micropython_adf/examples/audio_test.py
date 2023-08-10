# audio_test.py - Test audio module
#
# This code is in the Public Domain (or CC0 licensed, at your option.)
# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
#

import time
from audio import player, recorder

def wait_playing(p):
    i=1
    while p.get_state()['status'] == player.STATUS_RUNNING:
        print('\rPlaying: %s' % ('*' * i), end='')
        time.sleep(1)
        i += 1
    print('\n')

def run():
    print('======================== Player ========================')

    def callback(stat):
        print(stat)
        pass

    mPlayer=player(callback)
    mPlayer.set_vol(40)
    print('Set volume to: ', mPlayer.get_vol())
    print('Play source: ', 'https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3')
    mPlayer.play('https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3')
    wait_playing(mPlayer)

    aRecorder=recorder()
    print('===================== Record AMR ====================')
    print('Recording voice to a AMR file in 10 seconds. Please say something via microphone.')
    aRecorder.start('/sdcard/1.amr', recorder.AMR)

    for i in range(0,10,1):
        i += 1
        if i==10:
            print('\rRecording: %s\n' % ('*' * i), end='')
        else:
            print('\rRecording: %s' % ('*' * i), end='')
        time.sleep(1)

    aRecorder.stop()

    print('Playback the recording file')
    mPlayer.play('file://sdcard/1.amr')
    wait_playing(mPlayer)

    print('============== Record AMR with maxtime ==============')
    print('Recording voice to a AMR file in 10 seconds. Please say something via microphone.')
    aRecorder.start('/sdcard/2.amr', recorder.AMR, 10)
    i = 0
    while aRecorder.is_running():
        print('\rRecording: %s' % ('*' * i), end='')
        i += 1
        time.sleep(1)
    print('\rRecording: %s\n' % ('*' * i), end='')

    print('Playback the recording file')
    mPlayer.play('file://sdcard/2.amr')
    wait_playing(mPlayer)

    print('========= Record WAV with maxtime and endcb =========')
    def recorder_endcb(rec):
        print('\nRecoder state: ', rec.is_running())

    print('Recording voice to a WAV file in 10 seconds. Please say something via microphone.')
    aRecorder.start('/sdcard/3.wav', recorder.WAV, 10, recorder_endcb)
    i = 0
    while aRecorder.is_running():
        print('\rRecording: %s' % ('*' * i), end='')
        i += 1
        time.sleep(1)
    print('Playback the recording file')
    mPlayer.play('file://sdcard/3.wav')
    wait_playing(mPlayer)
