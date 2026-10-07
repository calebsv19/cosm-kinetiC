#!/usr/bin/env python3
"""Verify the packaged local session's external Python prerequisite and MCP."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    args = parser.parse_args()
    if sys.version_info < (3, 9):
        raise SystemExit('kinetiC local sessions require Python 3.9 or newer on PATH')
    app = args.app.resolve()
    script = app / 'Contents/Resources/scripts/physics_sim_session.py'
    worker = app / 'Contents/MacOS/physics_sim_session_worker'
    if not script.is_file() or not worker.is_file():
        raise SystemExit('packaged session script or executable is missing')
    requests = [
        {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
        {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'},
        {'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call',
         'params': {'name': 'capabilities', 'arguments': {}}},
    ]
    with tempfile.TemporaryDirectory(prefix='physics-session-package-') as root:
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1',
                   PHYSICS_SIM_SESSION_WORKER=str(worker))
        result = subprocess.run(
            [sys.executable, str(script), '--root', root, '--worker', str(worker), '--mcp'],
            input=''.join(json.dumps(row) + '\n' for row in requests),
            text=True, capture_output=True, timeout=30, env=env, check=True)
    replies = [json.loads(line) for line in result.stdout.splitlines()]
    if [row.get('id') for row in replies] != [1, 2, 3] or any('error' in row for row in replies):
        raise SystemExit('packaged session MCP handshake failed')
    names = {row['name'] for row in replies[1]['result']['tools']}
    if not {'capabilities', 'run_start', 'run_control', 'run_sample'} <= names:
        raise SystemExit('packaged session MCP tools are incomplete')
    if replies[2]['result'].get('isError'):
        raise SystemExit('packaged session capabilities failed')
    print(json.dumps({'status': 'passed', 'python': sys.executable,
                      'python_version': sys.version.split()[0],
                      'prerequisite': 'Python >=3.9 on user launch PATH',
                      'mcp_tools': sorted(names)}))


if __name__ == '__main__':
    main()
