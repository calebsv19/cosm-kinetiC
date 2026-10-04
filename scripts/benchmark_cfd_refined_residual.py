#!/usr/bin/env python3
"""Matched serial transient cost for true-residual checks within GMRES restarts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for repeat in range(3):
        for label, binary in [('baseline',args.baseline), ('candidate',args.candidate)]:
            start = time.perf_counter()
            result = subprocess.run([str(binary.resolve()),'16','.005'], capture_output=True,
                                    text=True, check=True, timeout=60)
            elapsed = time.perf_counter()-start
            path = args.output/f'{label}-{repeat}.log'
            path.write_text(result.stdout+result.stderr)
            values = {k:float(v) for k,v in re.findall(r'(\w+)=([^\s]+)', result.stdout)}
            rows.append(dict(label=label, wall_seconds=elapsed, values=values,
                             log_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
            print(label,repeat,elapsed,flush=True)
    reference = rows[0]['values']
    for row in rows:
        assert row['values']['max_divergence'] < 1e-8
        for key in ('velocity_l2','pressure_l2'):
            assert abs(row['values'][key]-reference[key]) < 1e-9
    medians = {label:statistics.median(r['wall_seconds'] for r in rows if r['label']==label)
               for label in ('baseline','candidate')}
    report = dict(schema='physics_sim_refined_residual_cost_v1', rows=rows,
                  medians_seconds=medians, speedup=medians['baseline']/medians['candidate'],
                  binary_sha256={label:hashlib.sha256(binary.read_bytes()).hexdigest()
                                 for label,binary in [('baseline',args.baseline),('candidate',args.candidate)]},
                  scope='Identical manufactured transient; physical residual tolerance unchanged; error-norm parity checked, not full-field equality')
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(medians),flush=True)


if __name__ == '__main__': main()
