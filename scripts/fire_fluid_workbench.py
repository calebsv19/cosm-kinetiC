#!/usr/bin/env python3
"""Loopback-only authoring panel for native Fire–Fluid scenes and saved playback."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import subprocess
from pathlib import Path
import threading

from coupling.workbench import Workbench, DEFAULTS


def handler(workbench, renderer=None):
    class Handler(BaseHTTPRequestHandler):
        def send(self, status, value, content_type='application/json'):
            data = json.dumps(value, allow_nan=False).encode() if content_type == 'application/json' else value
            self.send_response(status); self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(data))); self.send_header('Cache-Control', 'no-store')
            self.end_headers(); self.wfile.write(data)

        def do_GET(self):
            if not self.local_host(): return
            try:
                if self.path == '/':
                    self.send(200, (Path(__file__).parent / 'coupling/workbench.html').read_bytes(), 'text/html; charset=utf-8')
                elif self.path == '/state': self.send(200, workbench.state())
                elif self.path.startswith('/preview/'):
                    from cfd_evidence import admitted_path
                    path = admitted_path(workbench.root / self.path.removeprefix('/preview/'))
                    if not path.is_relative_to(workbench.root / 'exports') or path.suffix != '.bmp' or path.stat().st_size > 2 * 1024 * 1024:
                        raise ValueError('Contained preview required')
                    self.send(200, path.read_bytes(), 'image/bmp')
                else: self.send(404, {'error': 'Unknown route'})
            except (ValueError, OSError) as error: self.send(400, {'error': str(error)})

        def local_host(self):
            port = self.server.server_port
            if self.headers.get('Host') not in ('127.0.0.1:%d' % port, 'localhost:%d' % port):
                self.send(403, {'error': 'Use the local panel address'}); return False
            return True

        def do_POST(self):
            if not self.local_host(): return
            # Local trusted operator surface; no cross-origin browser mutations.
            origin = self.headers.get('Origin')
            expected = 'http://' + self.headers.get('Host', '')
            if origin and origin != expected:
                self.send(403, {'error': 'Use the local panel origin'}); return
            if self.headers.get('Content-Type') != 'application/json':
                self.send(400, {'error': 'JSON command required'}); return
            try:
                size = int(self.headers.get('Content-Length', 0))
                if not 0 < size <= 4096: raise ValueError('Bounded command required')
                body = json.loads(self.rfile.read(size))
                if self.path == '/create': workbench.create(body)
                elif self.path == '/resume': workbench.resume(body['run_id'])
                elif self.path == '/play': workbench.play()
                elif self.path == '/pause': workbench.pause()
                elif self.path == '/step':
                    workbench.pause(); workbench.step(body['revision'], body['operation_id'])
                elif self.path == '/export': self.send(200, workbench.export()); return
                elif self.path == '/render':
                    if renderer is None: raise ValueError('Launch with --renderer to enable local previews')
                    from coupling.render_preview import render
                    with workbench.lock:
                        run_id = workbench.run_id
                        exported = workbench.export()
                    receipt = render(exported['output'], renderer, [body['revision']])
                    image = Path(exported['output']) / receipt['frames'][0]['image']
                    image_url = '/preview/' + str(image.relative_to(workbench.root))
                    preview = {'image_url': image_url, 'revision': body['revision'],
                               'time_s': receipt['frames'][0]['time_s'], 'elapsed_s': receipt['elapsed_s']}
                    (workbench.root / 'runs' / run_id / 'preview.json').write_text(json.dumps(preview) + '\n')
                    self.send(200, {'receipt': receipt, 'image_url': image_url}); return
                else: self.send(404, {'error': 'Unknown command'}); return
                self.send(200, workbench.state())
            except (ValueError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error: self.send(400, {'error': str(error)})

        def log_message(self, format, *args):
            print(format % args, flush=True)
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'growth-root', 'fire-worker', 'fluid-worker', 'clock-worker'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--port', type=int, default=8841)
    parser.add_argument('--renderer', type=Path, help='existing local RayTracing headless binary; no source edits')
    args = parser.parse_args()
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(args.root.resolve() / 'native-evidence')
    workbench = Workbench(args.root, (args.growth_root, args.fire_worker, args.fluid_worker, args.clock_worker))
    if workbench.host is None: workbench.create(DEFAULTS)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), handler(workbench, args.renderer))
    thread = threading.Thread(target=workbench.run_loop, daemon=True); thread.start()
    print('Fire–Fluid panel: http://127.0.0.1:%d' % server.server_port, flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally:
        workbench.close(); thread.join(timeout=65); server.server_close()


if __name__ == '__main__': main()
