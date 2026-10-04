#!/usr/bin/env python3
"""Serial fixed-reference accuracy/cost screen; unsuccessful controls remain visible."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import sys
import time


def run(binary, root, label, n, uniform, depth, cap, timeout):
    args = [str(binary.resolve()), str(n), str(uniform), str(depth), '0', '4', str(cap)]
    clock = ['-l'] if sys.platform == 'darwin' else ['-v']
    start = time.perf_counter()
    child = subprocess.Popen(['/usr/bin/time', *clock, *args], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True, start_new_session=True)
    expired = False
    try:
        out, err = child.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        expired = True
        os.killpg(child.pid, signal.SIGKILL)
        out, err = child.communicate()
    elapsed = time.perf_counter() - start
    log = root / (label + '.log')
    log.write_text(out + err)
    pattern = (r'(\d+)\s+maximum resident set size' if sys.platform == 'darwin'
               else r'Maximum resident set size \(kbytes\):\s*(\d+)')
    match = re.search(pattern, err)
    rss = int(match[1]) * (1 if sys.platform == 'darwin' else 1024) if match else None
    values = {}
    for line in out.splitlines():
        if line.startswith(('n=', 'energy_strain=', 'numerical_memory_limit_bytes=',
                            'fixed_stokes_obstacle_physical_gate=', 'mesh_cpu_s=')):
            for key, value in re.findall(r'(\w+)=([^ ]+)', line):
                try: values[key] = float(value)
                except ValueError: values[key] = value
    qualified = (not expired and child.returncode == 0 and
                 values.get('fixed_stokes_obstacle_physical_gate') == 'passed')
    return dict(label=label, arguments=args[1:], exit_code=child.returncode,
                timed_out=expired, wall_seconds=elapsed, peak_rss_bytes=rss,
                physical_gate_passed=qualified, observations=values,
                log=str(log), log_sha256=hashlib.sha256(log.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    if not 0 < args.timeout <= 600: parser.error('timeout must be in (0,600] seconds')
    args.output.mkdir(parents=True, exist_ok=True)
    report = dict(schema='physics_sim_refined_accuracy_cost_v1',
                  scope='Fixed confined steady Stokes rectangle; separate 2% component/total/energy gate',
                  binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  platform=sys.platform, timeout_seconds=args.timeout, rows=[],
                  matched_accuracy_speedup=None, matched_accuracy_comparison_established=False)
    # Sequential execution, three local repeats; uniform controls retain identical
    # physical equations, force observation, reference and linear acceptance.
    cases = [('local-64-'+str(i),64,0,7,192) for i in range(3)]
    cases += [('uniform-64',64,1,0,768), ('uniform-128',128,1,0,768)]
    for case in cases:
        row = run(args.binary, args.output, *case, args.timeout)
        report['rows'].append(row)
        (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({k:row[k] for k in ('label','wall_seconds','peak_rss_bytes','physical_gate_passed','timed_out')}), flush=True)
    local = report['rows'][:3]
    controls = [r for r in report['rows'][3:] if r['physical_gate_passed']]
    if all(r['physical_gate_passed'] for r in local) and controls:
        report['matched_accuracy_comparison_established'] = True
        report['matched_accuracy_speedup'] = min(r['wall_seconds'] for r in controls)/statistics.median(r['wall_seconds'] for r in local)
        report['timing_scope'] = 'Three local repeats; single uniform samples, not a repeated-control speedup certificate'
    else:
        report['timing_scope'] = 'No matched qualifying pair; do not infer a matched-accuracy speedup'
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    if not all(r['physical_gate_passed'] for r in local):
        raise SystemExit('Local reference regression failed; inspect retained evidence')


if __name__ == '__main__': main()
