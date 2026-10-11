import http.server
import pathlib
import subprocess
import sys
import tempfile
import threading


class Server(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        if self.path == '/length':
            self.send_header('Content-Length', '16')
        self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(b'0123456789abcdef')

    def log_message(self, *args):
        pass


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Server)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    with tempfile.TemporaryDirectory(prefix='monchi-download-test-') as folder:
        target = pathlib.Path(folder) / 'download.part'
        for route, limit, expected in [('length', 8, 21), ('stream', 8, 21),
                                       ('length', 16, 0), ('stream', 16, 0)]:
            target.unlink(missing_ok=True)
            url = f'http://127.0.0.1:{server.server_port}/{route}'
            result = subprocess.run([sys.argv[1], url, str(target), str(limit)])
            assert result.returncode == expected, (route, limit, result.returncode)
            if expected == 0:
                assert target.read_bytes() == b'0123456789abcdef'
            else:
                assert not target.exists()
finally:
    server.shutdown()
    server.server_close()
print('Known-length and streamed download limits passed')
