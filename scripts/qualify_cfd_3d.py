#!/usr/bin/env python3
"""Finite local 3D verification. No release, worker fleet, or desktop actions."""
import argparse
import array
import json
import math
from pathlib import Path
import subprocess
import time


def rms_diff(a,b):
    return math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary',default='build/c3d/periodic3d_test')
    parser.add_argument('--output',default='build/c3d')
    args=parser.parse_args();root=Path(args.output);root.mkdir(parents=True,exist_ok=True)
    rows=[];fields=[]
    for dt in (.04,.02,.01,.005,.0025):
        field=root/f'temporal-{dt}.bin';start=time.monotonic()
        p=subprocess.run([args.binary,'16',str(dt),str(field)],capture_output=True,text=True,check=True)
        row=json.loads(p.stdout);row['wall_s']=time.monotonic()-start
        row['process_peak_rss_bytes']=int(p.stderr.strip().split('=')[-1]);rows.append(row)
        data=array.array('d');data.frombytes(field.read_bytes());fields.append(data)
    n=16**3
    changes=[]
    for a,b in zip(fields,fields[1:]):
        changes.append({'velocity_l2_m_s':rms_diff(a[:3*n],b[:3*n]),'pressure_l2_pa':rms_diff(a[3*n:],b[3*n:])})
    orders=[]
    for a,b in zip(changes,changes[1:]):
        orders.append({k:math.log(a[k]/b[k],2) for k in a})
    # Self-convergence isolates temporal error from the nonzero spatial floor.
    passed=all(o['velocity_l2_m_s']>=1.8 and o['pressure_l2_pa']>=1.8 for o in orders)
    spatial=[json.loads(line) for line in (root/'transient-spatial.jsonl').read_text().splitlines()]
    spatial_orders=[{k:math.log(a[k]/b[k],2) for k in ('velocity_l2_error_m_s','pressure_l2_error_pa')}
                    for a,b in zip(spatial,spatial[1:])]
    spatial_passed=all(v>=1.8 for row in spatial_orders for v in row.values())
    duct=[json.loads(line) for line in (root/'duct.jsonl').read_text().splitlines()]
    finest=duct[2]
    duct_passed=(finest['accepted_finest'] and finest['velocity_relative_l2']<=.01 and finest['pressure_relative_error']<=.01 and finest['max_wall_relative_error']<=.02 and finest['energy_relative_error']<=.02)
    result={'duct':duct,'duct_passed':duct_passed,'schema':'physics_sim_c3d_transient_qualification_v1','reference':'continuous unequal-length 3D manufactured Navier-Stokes',
            'spatial':spatial,'spatial_orders':spatial_orders,'temporal':rows,'temporal_changes':changes,
            'temporal_orders':orders,'temporal_method':'successive same-grid fields at matched t=0.4; isolates spatial floor',
            'spatial_passed':spatial_passed,'temporal_passed':passed,'passed':passed and spatial_passed and duct_passed,
            'scope':'periodic constant-coefficient three-component manufactured flow only'}
    (root/'transient-qualification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'spatial_orders':spatial_orders,'temporal_orders':orders,'passed':result['passed']}))
    if not result['passed']:raise SystemExit(1)

if __name__=='__main__':main()
