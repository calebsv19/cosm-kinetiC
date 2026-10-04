#!/usr/bin/env python3
"""Run declared C3D-8 reference meshes with a mesh, own-process RSS and time cap."""
import argparse
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--length', type=float, default=8)
    parser.add_argument('--center', type=float, default=4)
    parser.add_argument('--levels', type=int, nargs='+', default=[9, 10])
    args = parser.parse_args()
    assert args.length in (4, 6, 8) and args.center in (2, 4)
    assert all(0 <= level <= 11 for level in args.levels)
    root = ROOT / 'build/c3d-obstacle'
    root.mkdir(parents=True, exist_ok=True)
    receipts = []
    for level in args.levels:
        stem = f'reference8-L{args.length:g}-cx{args.center:g}-edge-l{level}'
        output = root / (stem + '.json')
        assert not output.exists(), f'preserve existing evidence: {output}'
        command = [str(ROOT / 'build/cfd-reference-venv/bin/python'),
                   str(ROOT / 'scripts/cfd_fem_reference3d.py'), '--n', '8',
                   '--length', str(args.length), '--center', str(args.center),
                   '--levels', str(level), '--edges-only', '--output', str(output)]
        start = time.monotonic()
        peak = 0
        reason = None
        with (root / (stem + '.log')).open('w') as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
            try:
                while process.poll() is None:
                    raw = subprocess.run(['/bin/ps', '-o', 'rss=', '-p', str(process.pid)],
                                         capture_output=True, text=True, check=True).stdout.strip()
                    rss = int(raw or '0') * 1024
                    peak = max(peak, rss)
                    if rss > 1800 * 1024 * 1024 or time.monotonic() - start > 180:
                        reason = 'own-process RSS or wall-time cap'
                        process.terminate()
                        break
                    time.sleep(.25)
            finally:
                if process.poll() is None:
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
        receipt = dict(command=command, returncode=process.returncode,
                       observed_rss_bytes=peak, elapsed_s=time.monotonic() - start,
                       stop_reason=reason, mesh_cap=50000, rss_cap_bytes=1800*1024*1024,
                       time_cap_s=180, output=str(output))
        receipts.append(receipt)
        (root / (stem + '-receipt.json')).write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt), flush=True)
        if process.returncode:
            raise SystemExit(process.returncode)


if __name__ == '__main__':
    main()
