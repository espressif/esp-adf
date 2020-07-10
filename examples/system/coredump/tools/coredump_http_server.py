#!/usr/bin/env python

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

"""
Core Dump HTTP Server:

Version 1.0

This script provide a HTTP server for Core dump.
This server can receive the coredump raw data from device and parse it with 'espcoredump.py'.
Please refer to 'https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/core_dump.html' for more information about 'Core Dump' and the 'espcoredump.py'

Generic command syntax:
    - --elf, -e ELF. Path to application's ELF file
    - --rom-elf, -r ROM_ELF. Path to ROM ELF file to use. Download from: https://dl.espressif.com/dl/esp32_rom.elf.

"""


import urlparse
import time
import SimpleHTTPServer, BaseHTTPServer
import sys, os
import argparse
import threading

try:
    sys.path.append(os.environ['IDF_PATH'] + '/components/espcoredump')
    import espcoredump
except ValueError:
    print ("Please set IDF_PATH first")
except ImportError:
    print ("Please check the IDF version")

__version__ = '1.0'

HOST = '0.0.0.0'
PORT = 8000

def get_coredump_request_handler(args):
    class CoreDumpRequestHander(SimpleHTTPServer.SimpleHTTPRequestHandler):

        def __init__(self, request, client_addr, server):
            self.args = args
            SimpleHTTPServer.SimpleHTTPRequestHandler.__init__(self, request, client_addr, server)

        def __coredump_save(self, data):
            f_name = time.strftime("%Y%m%d_", time.localtime()) + time.strftime("%H%M%S", time.localtime()) + '.coredump'
            with open(f_name, 'wb') as f:
                f.write(data)
                f.flush()
                f.close()
            return f_name

        def __coredump_parse(self, fname):

            self.args.debug = 0
            self.args.gdb = 'xtensa-esp32-elf-gdb'
            self.args.core = fname
            self.args.core_format = 'raw'
            # self.args.off = 0x110000
            self.args.save_core = ''
            self.args.prog = self.args.elf
            self.args.print_mem = False
            espcoredump.info_corefile(self.args)

        def do_POST(self):
            response = 200
            request_path = urlparse.urlparse(self.path).path.strip('/')
            if request_path == 'upload':
                if (self.headers['Content-Type'] == 'application/octet-stream'):
                    data = self.rfile.read(int(self.headers['Content-Length']))
                    f_name = self.__coredump_save(data)
                    self.__coredump_parse(f_name)
            else:
                response = 404

            self.send_response(response)
            self.end_headers()
            self.rfile.close()
            self.wfile.close()

    return CoreDumpRequestHander


class CoreDumpServer(BaseHTTPServer.HTTPServer, threading.Thread):

    def __init__(self, args):
        BaseHTTPServer.HTTPServer.__init__(self, (HOST, PORT), get_coredump_request_handler(args))
        threading.Thread.__init__(self)

    def run(self):
        print("Serving HTTP on {} port {}".format(HOST, PORT))
        self.serve_forever()

    def stop(self):
        self.shutdown()

def main():
    argparser = argparse.ArgumentParser(description='server.py v%s - Coredump http server' % __version__)
    argparser.add_argument('-e', '--elf', type=str, help='path to the elf file')
    argparser.add_argument('-r', '--rom-elf', type=str, help='path to ROM ELF file.')

    args = argparser.parse_args()

    coredump_server = CoreDumpServer(args)
    coredump_server.run()

if __name__ == "__main__":
    main()