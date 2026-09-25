#!/usr/bin/env python3
# localhost server for the wasm preview.
# Sends COOP/COEP so the page may use SharedArrayBuffer (needed by
# web/worker.js key queue), correct wasm MIME, and maps /disk.img to
# the repo disk image built by `make disk.img`.
import http.server
import os
import sys

WEB = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(WEB)


class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def do_GET(self):
        if self.path.split('?')[0] == '/disk.img':
            p = os.path.join(ROOT, 'disk.img')
            if not os.path.exists(p):
                self.send_error(404, 'disk.img missing (run: make disk.img)')
                return
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Length', str(os.path.getsize(p)))
            self.end_headers()
            if self.command == 'GET':
                with open(p, 'rb') as f:
                    self.wfile.write(f.read())
            return
        return super().do_GET()

    do_HEAD = do_GET


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    os.chdir(WEB)
    with http.server.ThreadingHTTPServer(('127.0.0.1', port), H) as d:
        print(f'inaneos wasm preview on http://localhost:{port}/')
        d.serve_forever()
