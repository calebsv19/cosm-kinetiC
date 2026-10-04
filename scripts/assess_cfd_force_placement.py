#!/usr/bin/env python3
"""Assess fixed-geometry control-surface/time sensitivity, not drag accuracy."""
import argparse
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
rows = [{k: float(v) for k, v in re.findall(r'(\w+)=([^ ]+)', line)}
        for line in args.log.read_text().splitlines() if line.startswith('cvcheck ')]
expected = {(n, t, k) for n in (16, 32, 64) for t in (5, 10) for k in range(4)}
actual = {(r['n'], r['time'], r['placement']) for r in rows}
if actual != expected or len(rows) != len(expected):
    raise SystemExit('Incomplete or duplicate placement campaign')
summaries = []
for n in (16, 32, 64):
    for t in (5, 10):
        group = [r for r in rows if r['n'] == n and r['time'] == t]
        forces = [r['force'] for r in group]
        summaries.append({'n': n, 'time': t,
            'relative_placement_spread': (max(forces)-min(forces))/max(forces),
            'max_relative_momentum_rate': max(abs(r['momentum_rate']/r['force']) for r in group),
            'min_traction_mismatch': min(r['mismatch'] for r in group),
            'max_traction_mismatch': max(r['mismatch'] for r in group)})
changes = []
for n in (16, 32, 64):
    a, b = [next(r for r in rows if r['n'] == n and r['time'] == t and r['placement'] == 0)
            for t in (5, 10)]
    changes.append({'n': n, 'relative_force_change_5_to_10': abs(b['force']-a['force'])/abs(b['force'])})
passed = (all(r['relative_placement_spread'] < .005 for r in summaries)
          and all(r['relative_force_change_5_to_10'] < .01 for r in changes))
report = {'schema': 'physics_sim_force_placement_v1', 'placement_spread_limit': .005,
          'endpoint_force_change_limit': .01, 'sensitivity_screen_passed': passed,
          'scope': 'Fixed periodic rectangle; endpoint screen is not a steady-state proof',
          'force_accuracy_qualified': False, 'summaries': summaries, 'time_changes': changes, 'rows': rows}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k != 'rows'}, indent=2))
raise SystemExit(0 if passed else 1)
