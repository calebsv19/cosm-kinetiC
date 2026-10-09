#!/usr/bin/env python3
"""Verify the packaged local session's external Python prerequisite and MCP."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--output-root',type=Path)
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
    root = Path(os.path.abspath(args.output_root)) if args.output_root else Path(tempfile.mkdtemp(prefix='physics-session-package-'))
    if args.output_root:
        current=root
        while current!=current.parent:
            if current.is_symlink():raise SystemExit('Session proof refuses symlink output components')
            current=current.parent
        root.mkdir(parents=True,exist_ok=False)
    print('Packaged session proof retained: '+str(root),file=sys.stderr)
    payload=''.join(json.dumps(row)+'\n' for row in requests)
    (root/'requests.jsonl').write_text(payload)
    # Evidence may live in a source checkout; a packaged session's runtime may not.
    runtime = Path(tempfile.mkdtemp(prefix='physics-packaged-session-runtime-')).resolve()
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PHYSICS_SIM_SESSION_WORKER=str(worker))
    try:
        result = subprocess.run(
            [sys.executable, str(script), '--root', str(runtime/'session'), '--worker', str(worker), '--mcp'],
            input=payload,text=True,capture_output=True,timeout=30,env=env)
    except subprocess.TimeoutExpired as error:
        for name,content in (('stdout.log',error.stdout),('stderr.log',error.stderr)):
            (root/name).write_text(content.decode(errors='replace') if isinstance(content,bytes) else content or '')
        (root/'execution.json').write_text(json.dumps({'state':'timed_out','output_root':str(root),'runtime_root':str(runtime)}))
        raise SystemExit('Packaged session proof timed out; retained '+str(root))
    (root/'stdout.log').write_text(result.stdout);(root/'stderr.log').write_text(result.stderr)
    (root/'execution.json').write_text(json.dumps({'state':'command_completed','exit_code':result.returncode,'output_root':str(root),'runtime_root':str(runtime)}))
    if result.returncode:
        raise SystemExit('Packaged session command failed; retained '+str(root))
    replies = [json.loads(line) for line in result.stdout.splitlines()]
    if [row.get('id') for row in replies] != [1, 2, 3] or any('error' in row for row in replies):
        raise SystemExit('packaged session MCP handshake failed')
    names = {row['name'] for row in replies[1]['result']['tools']}
    if not {'capabilities', 'run_start', 'run_control', 'run_sample'} <= names:
        raise SystemExit('packaged session MCP tools are incomplete')
    if replies[2]['result'].get('isError'):
        raise SystemExit('packaged session capabilities failed')
    shutil.rmtree(runtime)
    print(json.dumps({'status': 'passed', 'python': sys.executable,
                      'python_version': sys.version.split()[0],
                      'prerequisite': 'Python >=3.9 on user launch PATH',
                      'mcp_tools': sorted(names),'output_root':str(root)}))


if __name__ == '__main__':
    main()
