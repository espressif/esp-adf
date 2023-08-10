# install_libs.py - install libs to SDCard
#
# This code is in the Public Domain (or CC0 licensed, at your option.)
# Unless required by applicable law or agreed to in writing, this
# software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
#

import mip

def install():
    mip.install('json', target='/sdcard/lib')
    mip.install('urequests', target='/sdcard/lib')
    mip.install('hmac', target='/sdcard/lib')
