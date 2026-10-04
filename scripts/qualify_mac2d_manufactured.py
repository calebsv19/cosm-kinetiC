#!/usr/bin/env python3
import json, subprocess, math, hashlib
from pathlib import Path
out=Path('build/s3-mac2d-manufactured');out.mkdir(parents=True,exist_ok=True)
report={}
for scheme in ('upwind','centered','limited'):
    binary='build/cfd_mac2d_manufactured_'+scheme
    for name,a,p,b in [('coupled',.01,.01,0),('pressure_only',0,.01,0),('wall_gradient',0,.01,1)]:
        runs=[]
        for n,steps in [(8,400),(16,400),(32,400),(64,400),(32,100),(32,200),(32,800),(64,800)]:
            runs.append(json.loads(subprocess.check_output([binary,str(n),str(steps),str(a),str(p),str(b)],text=True)))
        report[scheme+'_'+name]=runs
checks = {
    'limited_velocity_decreases': all(report['limited_coupled'][i+1]['velocity_l2'] < report['limited_coupled'][i]['velocity_l2'] for i in range(3)),
    'limited_fine_velocity_improves_5x': report['limited_coupled'][3]['velocity_l2'] < report['upwind_coupled'][3]['velocity_l2']/5,
    'pressure_spatial_second_order_after_time_alignment': 3.8 < report['upwind_pressure_only'][2]['pressure_start_time_l2']/report['upwind_pressure_only'][3]['pressure_start_time_l2'] < 4.2,
    'pressure_only_transport_neutral': all(abs(report['upwind_pressure_only'][i]['pressure_l2']-report['limited_pressure_only'][i]['pressure_l2']) < 1e-10 for i in range(8)),
    'wall_gradient_spatial_refinement': all(report['upwind_wall_gradient'][i+1]['velocity_l2'] < report['upwind_wall_gradient'][i]['velocity_l2'] for i in range(3)),
}
artifact = {'schema':'physics_sim_mac2d_manufactured_v1','checks':checks,'passed':all(checks.values()),'cases':report,
            'scope':'smooth forced laminar verification; test-only transport variants; no obstacle/outlet/force acceptance',
            'binary_sha256':{scheme:hashlib.sha256(Path('build/cfd_mac2d_manufactured_'+scheme).read_bytes()).hexdigest() for scheme in ('upwind','centered','limited')}}
(out/'report.json').write_text(json.dumps(artifact,indent=2)+'\n')
for name,r in report.items():
    print(name,[(x['n'],round(x['velocity_l2'],9),round(x['pressure_l2'],9)) for x in r[:4]])

print(checks)
raise SystemExit(0 if all(checks.values()) else 1)
