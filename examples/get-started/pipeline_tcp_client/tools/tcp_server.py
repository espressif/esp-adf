#!/user/bin/env python

#  ESPRESSIF MIT License
#
#  Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
#
#  Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
#  it is free of charge, to any person obtaining a copy of this software and associated
#  documentation files (the "Software"), to deal in the Software without restriction, including
#  without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the Software is furnished
#  to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all copies or
#  substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
#  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
#  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import socket
import os

RCV_BUF_SIZE = 8194
SND_BUF_SIZE = 8194
SERVER_PORT  = 8080
read_size = 0

FILE_PATH = "./"
FILE_NAME = "esp32.mp3"

def log_info(value):
    print("\033[35m[%s][user]%s\033[0m"%(get_time_stamp(),value))

def start_tcp_server(ip, port):

    fo = open("esp32.mp3", "r+")
    fsize = os.path.getsize(FILE_PATH+FILE_NAME)
    print ("Get the %s size is %d" % (FILE_NAME, fsize))

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_address = (ip, port)

    print("starting listen on ip %s, port %s" % server_address)
    sock.bind(server_address)

    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, RCV_BUF_SIZE)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SND_BUF_SIZE)

    try:
        sock.listen(1)
    except socket.error:
        print("fail to listen on port %s" % e)
        sys.exit(1)

    print("waiting for client to connect")
    client, addr = sock.accept()
    send_msg = "get msg, will download"
    client.send(send_msg)
    global read_size
    while True:
        file_msg = fo.read(1024)
        read_size += len(file_msg)
        print('total size \033[1;35m [%d/%d] \033[0m' % (read_size, fsize))
        if (len(file_msg) <= 0):
            print("get all data for %s" % FILE_NAME)
            break
        client.send(file_msg)

    fo.close()
    client.close()
    sock.close()
    print(" close client connect ")

if __name__=='__main__':
    start_tcp_server(socket.gethostbyname(socket.getfqdn(socket.gethostname())),SERVER_PORT)
