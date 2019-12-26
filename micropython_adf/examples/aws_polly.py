# aws_polly.py - aws polly service test
#
# This code is in the Public Domain (or CC0 licensed, at your option.)
# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
#

import ntptime
import utime
import time
import uhashlib
import ubinascii
import hmac
import ujson
import urequests
from audio import player

tts_txt = b'Espressif Systems is a multinational, fabless semiconductor company, with headquarters in Shanghai, China. We specialize in producing highly-integrated, low-power, WiFi-and-Bluetooth IoT solutions. Among our most popular chips are ESP8266 and ESP32. We are committed to creating IoT solutions that are power-efficient, robust and secure.'
polly_payload = b'{"OutputFormat":"mp3","SampleRate":"22050","Text":"' + tts_txt + b'","TextType":"text","VoiceId":"Joanna"}'

class aws_config():
    def __init__(self, region, key, secret, payload, amz_date, date_stamp):
        self.service_name = b'polly'
        self.region = region
        self.key = key
        self.secret = secret
        self.host = b"polly." + region + b".amazonaws.com"
        self.method = b"POST"
        self.path = b"/v1/speech"
        self.query = b""
        self.signed_headers = b"content-type"
        self.canonical_headers = b"content-type:application/json\n"
        self.payload = payload
        self.amz_date = amz_date
        self.date_stamp = date_stamp

    def signing_header(self):
        # payload_hash
        payload_hash = ubinascii.hexlify(uhashlib.sha256(self.payload).digest()).decode("utf-8")
        # separate
        if self.signed_headers and len(self.signed_headers) > 0:
            separate = b';'
        else:
            separate = 0
        # canonical_request_sha256
        canonical_request = b"%s\n%s\n%s\n%shost:%s\nx-amz-date:%s\n\n%s%shost;x-amz-date\n%s" % \
            (self.method, self.path, self.query, self.canonical_headers, self.host, self.amz_date, self.signed_headers, separate, payload_hash)
        canonical_request_sha256 = ubinascii.hexlify(uhashlib.sha256(canonical_request).digest()).decode("utf-8")
        # credential_scope
        credential_scope = b"%s/%s/%s/aws4_request" % (self.date_stamp, self.region, self.service_name)
        # signing_key
        aws4_key = b"AWS4%s" % (self.secret)
        k_date = hmac.new(aws4_key, self.date_stamp, digestmod=uhashlib.sha256).digest()
        k_region = hmac.new(k_date, self.region, digestmod=uhashlib.sha256).digest()
        k_service = hmac.new(k_region, self.service_name, digestmod=uhashlib.sha256).digest()
        signing_key = hmac.new(k_service, b'aws4_request', digestmod=uhashlib.sha256).digest()

        string_to_sign = b"%s\n%s\n%s\n%s" % (b"AWS4-HMAC-SHA256", self.amz_date, credential_scope, canonical_request_sha256)

        signature = ubinascii.hexlify(hmac.new(signing_key, string_to_sign, digestmod=uhashlib.sha256).digest()).decode("utf-8")

        authorization_header = b"%s Credential=%s/%s, SignedHeaders=%s%shost;x-amz-date, Signature=%s" % \
            (b"AWS4-HMAC-SHA256", self.key, credential_scope, self.signed_headers, separate, signature)

        return authorization_header

def query_utctime():
    time_get = False
    while time_get == False:
        try:
            ntptime.settime()
            time_get = True
        except:
            print('ntp timeout')
    print('current time is: ', utime.localtime())

def wait_playing(p):
    i=1
    while p.get_state()['status'] == player.STATUS_RUNNING:
        print('\rPlaying: %s' % ('*' * i), end='')
        time.sleep(1)
        i += 1
    print('\n')

def run(region = b'us-east-1', key = b'', secret = b''):
    query_utctime()
    amz_date = b'%d%02d%02dT%02d%02d%02dZ' % utime.localtime()[0 : 6]
    date_stamp = b'%d%02d%02d' % utime.localtime()[0 : 3]

    auth_header = aws_config(region, key, secret, polly_payload, amz_date, date_stamp).signing_header()
    aws_headers = {}
    aws_headers['content-type'] = b'application/json'
    aws_headers['x-amz-date'] = amz_date
    aws_headers['Authorization'] = auth_header

    url = "https://polly." + region.decode("utf-8") + ".amazonaws.com/v1/speech"
    print(url)
    resp = urequests.post(url, data = polly_payload, headers = aws_headers)
    print('status code: ', resp.status_code , '\n', 'reason code: ' , resp.reason, '\n', 'Content len: ', len(resp.content))
    mp3 = resp.content
    f = open('/sdcard/polly.mp3', 'w+')
    f.write(mp3)
    f.close()

    # Create player and play the tts result
    p = player(None)
    print('Play source: ', 'file://sdcard/polly.mp3')
    p.play('file://sdcard/polly.mp3')
    wait_playing(p)
    print('End')
