"""Solve an explicit stationary grid-aligned box and verify complete SI readback.

Source-checkout developer lane; named immutable inputs/logs/receipts. No absolute
force certification, arbitrary mesh import, inertia, package or worker activation.
"""
import argparse
import json
import math
from pathlib import Path
import re
from run_cfd_native_accuracy_regression import execute,require,save,sha

ROOT=Path(__file__).resolve().parents[1]
FILES=('scripts/run_cfd_native_box_accuracy.py','scripts/run_cfd_native_accuracy_regression.py',
 'tests/cfd_obstacle3d_box_accuracy_field_probe.c','include/app/cfd_obstacle3d_box.h',
 'src/app/cfd_obstacle3d_box.c','src/app/cfd_obstacle3d.c','src/app/cfd_obstacle3d_mixed.c',
 'src/app/cfd_obstacle3d_reconstruction.c','src/app/cfd_obstacle3d_pressure_trace.c',
 'src/app/cfd_cartesian3d.c','src/app/cfd_sparse_mg.c','src/app/cfd_memory.c',
 'include/app/cfd_obstacle3d.h','include/app/cfd_obstacle3d_pressure_trace.h',
 'include/app/cfd_mixed3d.h','include/app/cfd_cartesian3d.h','include/app/cfd_sparse_mg.h','include/app/cfd_memory.h')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--name',required=True)
 p.add_argument('--grid',type=int,nargs=3,required=True);p.add_argument('--length',type=float,default=4)
 p.add_argument('--lower-m',type=float,nargs=3,required=True);p.add_argument('--upper-m',type=float,nargs=3,required=True)
 args=p.parse_args();require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',args.name),'Run identity')
 require(all(4<=n<=256 for n in args.grid) and math.prod(args.grid)<=1048576,'Original grid admission')
 require(all(math.isfinite(x) for x in [args.length,*args.lower_m,*args.upper_m]),'Finite geometry')
 require(.001<=args.length<=1000,'Tunnel length')
 directory=ROOT/'build/c3d-box-accuracy/runs'/args.name;directory.mkdir(parents=True,exist_ok=False);frozen=directory/'source'
 sources={q:sha(ROOT/q) for q in FILES}
 for q,h in sources.items():
  f=frozen/q;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((ROOT/q).read_bytes());require(sha(f)==h,'Source freeze')
 contract=dict(grid=args.grid,length_m=args.length,lower_m=args.lower_m,upper_m=args.upper_m,
  density_kg_m3=1,viscosity_pa_s=.1,flow_m3_s=.008,source_sha256=sources,
  original_cell_cap=1048576,owned_cap_bytes=1024**3,rss_cap_bytes=1536*1024**2,
  fixture_wall_cap_s=1800,parent_wall_cap_s=1830,complete_momentum_max=1e-11,
  divergence_max=1e-8,flux_max=1e-9,discrete_momentum_max=1e-9,discrete_energy_max=1e-9,
  physical_energy_max=.03,physical_momentum_max=.02,physical_absolute_force_certification=False,
  performance_gate_applied=False)
 save(directory/'contract.json',contract)
 result=dict(status='failed',processes={},source_sha256=sources,physical_absolute_force_certification=False)
 try:
  cc=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY','-I'+str(frozen/'include'),
      str(frozen/'tests/cfd_obstacle3d_box_accuracy_field_probe.c')]
  cc += [str(frozen/q) for q in FILES if q.startswith('src/') and not q.endswith('cfd_obstacle3d_mixed.c')]
  cc += ['-lm','-o',str(directory/'probe')]
  result['processes']['compile']=execute(cc,directory,'compile',60,1024**3)
  command=[str(directory/'probe'),*map(str,args.grid),repr(args.length),*map(repr,args.lower_m),*map(repr,args.upper_m),str(directory/'field.bin')]
  result['processes']['field']=execute(command,directory,'field',1830,1536*1024**2)
  result['processes']['readback']=execute([str(directory/'probe'),'--readback',str(directory/'field.bin')],directory,'readback',180,1536*1024**2)
  row=json.loads((directory/'field.stdout').read_text());readback=json.loads((directory/'readback.stdout').read_text())
  for k in row:
   if k not in ('readback','wall_s'):
    require(row[k]==readback[k],'Complete field readback '+k)
  require(row['grid']==args.grid and row['length_m']==args.length,'Grid/domain identity')
  require(row['owner_peak_bytes']<=1024**3 and row['wall_s']<1800,'Owner resources')
  for expected,observed in zip(args.lower_m+args.upper_m,row['lower_m']+row['upper_m']):
   require(abs(expected-observed)<1e-12,'Actual body identity')
  result['control']=row;result['readback']=readback
  result['physical_balance_passed']=row['physical_energy_imbalance']<=.03 and row['physical_momentum_relative_max']<=.02
  for q,h in sources.items():require(sha(ROOT/q)==h==sha(frozen/q),'Final source identity '+q)
  result['status']='completed_native_box_with_complete_SI_readback'
 except Exception as error:result['failure']=str(error)
 result['artifact_sha256']={str(f.relative_to(directory)):sha(f) for f in directory.rglob('*') if f.is_file()}
 save(directory/'receipt.json',result);print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'),failure=result.get('failure'))))
 return 0 if result['status'].startswith('completed') else 1
if __name__=='__main__':raise SystemExit(main())
