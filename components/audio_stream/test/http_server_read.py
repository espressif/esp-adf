import os, datetime, sys, urlparse
import SimpleHTTPServer, BaseHTTPServer
import wave

PORT = 8000
HOST = '192.168.199.168'

class Handler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def _set_headers(self, length):
        self.send_response(200)
        if length > 0:
            self.send_header('Content-length', str(length))
        self.end_headers()

    def _get_chunk_size(self):
        data = self.rfile.read(2)
        while data[-2:] != b"\r\n":
            data += self.rfile.read(1)
        return int(data[:-2], 16)

    def _get_chunk_data(self, chunk_size):
        data = self.rfile.read(chunk_size)
        self.rfile.read(2)
        return data

    def _write_file(self, data):
        t = datetime.datetime.utcnow()
        time = t.strftime('%Y%m%dT%H%M%SZ')
        filename = str.format('{}.mp3', time)

        file = open(filename, 'wb')
        file.write(bytearray(data))
        file.close()
        return filename

    def do_POST(self):
        urlparts = urlparse.urlparse(self.path)
        request_file_path = urlparts.path.strip('/')
        total_bytes = 0
        if (request_file_path == 'upload'):
            data = []
            # https://stackoverflow.com/questions/24500752/how-can-i-read-exactly-one-response-chunk-with-pythons-http-client
            while True:
                chunk_size = self._get_chunk_size()
                total_bytes += chunk_size
                print("Total bytes received: {}".format(total_bytes))
                sys.stdout.write("\033[F")
                if (chunk_size == 0):
                    break
                else:
                    chunk_data = self._get_chunk_data(chunk_size)
                    data += chunk_data

            filename = self._write_file(data)
            body = 'File {} was written, size {}'.format(filename, total_bytes)
            self._set_headers(len(body))
        else:
            return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

httpd = BaseHTTPServer.HTTPServer((HOST, PORT), Handler)

print("Serving HTTP on {} port {}".format(HOST, PORT));
httpd.serve_forever()