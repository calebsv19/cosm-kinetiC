#!/usr/bin/env python3
"""Uniform/local cost at the same explicit continuous manufactured-flow error limits."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import sys
import time


def run(binary, n, label, output):
    clock = ['-l'] if sys.platform == 'darwin' else ['-v']
    start = time.perf_counter()
    child = subprocess.Popen(['/usr/bin/time', *clock, str(binary.resolve()), str(n), '.005'],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             start_new_session=True)
    try:
        out, err = child.communicate(timeout=120)
    except subprocess.TimeoutExpired:
        os.killpg(child.pid, signal.SIGKILL)
        out, err = child.communicate()
        (output/(label+'.log')).write_text(out+err)
        raise RuntimeError('bounded case timed out: '+label)
    elapsed = time.perf_counter()-start
    log = output/(label+'.log'); log.write_text(out+err)
    if child.returncode: raise RuntimeError('case failed: '+label)
    values = {k:float(v) for k,v in re.findall(r'(\w+)=([^\s]+)', out)}
    assert all(math.isfinite(v) for v in values.values())
    pattern = (r'(\d+)\s+maximum resident set size' if sys.platform == 'darwin'
               else r'Maximum resident set size \(kbytes\):\s*(\d+)')
    match = re.search(pattern, err); assert match, 'missing measured RSS'
    rss = int(match[1])*(1 if sys.platform=='darwin' else 1024)
    accepted = (values['velocity_l2'] <= 1e-4 and values['pressure_l2'] <= 1e-4
                and values['max_divergence'] < 1e-8)
    return dict(label=label, values=values, wall_seconds=elapsed, peak_rss_bytes=rss,
                accepted=accepted, log=str(log), log_sha256=hashlib.sha256(log.read_bytes()).hexdigest())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uniform', type=Path, required=True)
    p.add_argument('--local', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    report = dict(schema='physics_sim_matched_transient_accuracy_cost_v1',
                  scope='Smooth continuous manufactured 2D flow, 0.4s interval, constant dt .005s; no obstacle-force accuracy claim',
                  limits=dict(velocity_l2_m_s=1e-4, pressure_l2_pa=1e-4, max_divergence_s_inv=1e-8),
                  binary_sha256={k:hashlib.sha256(v.read_bytes()).hexdigest() for k,v in [('uniform',a.uniform),('local',a.local)]},
                  rows=[], matched_accuracy_qualified=False)
    cases=[('uniform-coarse',a.uniform,8),('local-coarse',a.local,16)]
    for repeat in range(3): cases += [(f'uniform-{repeat}',a.uniform,16),(f'local-{repeat}',a.local,32)]
    for label,binary,n in cases:
        row=run(binary,n,label,a.output); report['rows'].append(row)
        (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:row[k] for k in ('label','wall_seconds','peak_rss_bytes','accepted')}),flush=True)
    assert all(not row['accepted'] for row in report['rows'][:2]), 'coarser control already qualifies; revisit selected meshes'
    assert all(row['accepted'] for row in report['rows'][2:]), 'unmatched physical accuracy'
    report['matched_accuracy_qualified']=True
    report['median_wall_seconds']={kind:statistics.median(r['wall_seconds'] for r in report['rows'][2:] if r['label'].startswith(kind)) for kind in ('uniform','local')}
    report['median_peak_rss_bytes']={kind:statistics.median(r['peak_rss_bytes'] for r in report['rows'][2:] if r['label'].startswith(kind)) for kind in ('uniform','local')}
    report['interpretation']='Select based on measured physical-error cost; local refinement is not assumed to be faster for smooth fields'
    (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__': main()
