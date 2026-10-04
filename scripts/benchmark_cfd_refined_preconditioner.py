#!/usr/bin/env python3
"""Matched discrete-problem cost, not a physical-accuracy certificate."""
import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


def run(binary, n, root, name):
    clock = ['-l'] if sys.platform == 'darwin' else ['-v']
    start = time.perf_counter()
    result = subprocess.run(['/usr/bin/time', *clock, str(binary.resolve()), str(n)],
                            capture_output=True, text=True, timeout=180, check=True)
    wall = time.perf_counter() - start
    (root / f'{name}.log').write_text(result.stdout + result.stderr)
    line = next(line for line in result.stdout.splitlines() if line.startswith('n='))
    values = {key: float(value) for key, value in re.findall(r'(\w+)=([^ ]+)', line)}
    if sys.platform == 'darwin':
        match = re.search(r'(\d+)\s+maximum resident set size', result.stderr)
        rss = int(match[1]) if match else None
    else:
        match = re.search(r'Maximum resident set size \(kbytes\):\s*(\d+)', result.stderr)
        rss = 1024 * int(match[1]) if match else None
    assert rss is not None, 'Peak memory must be measured, not inferred from solver storage'
    return dict(variant=name.split('-')[0], n=n, wall_seconds=wall,
                peak_rss_bytes=rss, observations=values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--multilevel', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for repeat in range(3):
        for label, binary in [('ilu', args.baseline), ('multilevel', args.multilevel)]:
            rows.append(run(binary, 32, args.output, f'{label}-32-{repeat}'))
    reference = rows[0]['observations']
    for row in rows:
        for key in ('pressure', 'viscous', 'total'):
            assert abs(row['observations'][key] - reference[key]) < 1e-9
    for n in (64, 128):
        rows.append(run(args.multilevel, n, args.output, f'multilevel-{n}'))
    old = statistics.median(r['wall_seconds'] for r in rows if r['variant'] == 'ilu')
    new = statistics.median(r['wall_seconds'] for r in rows
                            if r['variant'] == 'multilevel' and r['n'] == 32)
    report = dict(schema='physics_sim_refined_preconditioner_cost_v1', platform=sys.platform,
                  matched_n32_wall_speedup=old/new, rows=rows,
                  scope='Same discrete problem; physical force qualification remains separate',
                  physical_accuracy_certified=False)
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(n32_speedup=old/new,
                         fine_iterations=[r['observations']['iterations'] for r in rows[-2:]])))


if __name__ == '__main__':
    main()
