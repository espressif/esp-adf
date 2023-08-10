# baidu_tts.py - baidu tts service test
#
# This code is in the Public Domain (or CC0 licensed, at your option.)
# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
#


# Get token of baidu
import ujson
import urequests
import time
from audio import player

def wait_playing(p):
    i=1
    while p.get_state()['status'] == player.STATUS_RUNNING:
        print('\rPlaying: %s' % ('*' * i), end='')
        time.sleep(1)
        i += 1
    print('\n')

def run(access, secret):
    url = 'https://openapi.baidu.com/oauth/2.0/token?grant_type=client_credentials&client_id=%s&client_secret=%s' % (access, secret)
    print('Request the access token')
    resp = urequests.get(url)
    print('Get Response')
    result = ujson.loads(resp.text)

    # Get tts result and write to file
    post_dat = bytearray('lan=zh&cuid=ESP32&ctp=1&tok=%s&tex=%s' % (result['access_token'], '欢迎使用乐鑫音频平台，想了解更多方案信息请联系我们'), 'utf8')
    resp = urequests.post('http://tsn.baidu.com/text2audio', data = post_dat)
    mp3 = resp.content
    print('Get content and save to /sdcard/tts.mp3')
    f = open('/sdcard/tts.mp3', 'w+')
    f.write(mp3)
    f.close()

    print('Create player')
    # Create player and play the tts result
    p = player(None)
    print('Play source: ', 'file://sdcard/tts.mp3')
    p.play('file://sdcard/tts.mp3')
    wait_playing(p)
    print('End')
