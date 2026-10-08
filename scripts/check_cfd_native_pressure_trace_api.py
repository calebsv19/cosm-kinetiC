"""Bounded complete field readback and strict original-equation verification."""
import argparse
import json
from pathlib import Path
import struct
import numpy as np
from cfd_run_support import compile_probe, execute,require,save,sha
ROOT=Path(__file__).resolve().parents[1]
from cfd_evidence import experiment_root, seal_bundle, verify_bundle, portable_paths
import shutil
FILES=('scripts/check_cfd_native_pressure_trace_api.py','scripts/cfd_run_support.py','scripts/cfd_evidence.py','scripts/check_clean_root.py',
 'tests/cfd_obstacle3d_pressure_trace_readback.c','tests/cfd_obstacle3d_wall_pressure_candidate.h',
 'src/app/cfd_obstacle3d_mixed.c','src/app/cfd_obstacle3d.c','src/app/cfd_obstacle3d_reconstruction.c',
 'src/app/cfd_cartesian3d.c','src/app/cfd_sparse_mg.c','src/app/cfd_memory.c',
 'src/app/cfd_obstacle3d_pressure_trace.c','include/app/cfd_obstacle3d_pressure_trace.h',
 'include/app/cfd_obstacle3d.h','include/app/cfd_mixed3d.h','include/app/cfd_cartesian3d.h','include/app/cfd_sparse_mg.h','include/app/cfd_memory.h')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('receipt',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
 receipt=args.receipt.resolve();row=json.loads(receipt.read_text());require(row['status']=='completed_native_cube_pressure_comparison','Successful terminal field')
 verify_bundle(receipt.parent)
 c=row['control'];directory=experiment_root(ROOT,args.output);require(not directory.is_relative_to(receipt.parent),'Trace output cannot mutate input bundle');directory.mkdir(parents=True,exist_ok=False);frozen=directory/'source'
 sources={q:sha(ROOT/q) for q in FILES}
 for q,h in sources.items():
  file=frozen/q;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes((ROOT/q).read_bytes());require(sha(file)==h,'Freeze drift')
 shutil.copy2(receipt,directory/'input-receipt.json');shutil.copy2(receipt.parent/'field.bin',directory/'field.bin')
 binary=directory/'field.bin';require(sha(binary)==row['artifact_sha256']['field.bin'],'Field identity')
 contract=dict(source_sha256=sources,input_receipt_sha256=sha(receipt),input_binary_sha256=sha(binary),owned_cap_bytes=1024**3,
  rss_cap_bytes=1536*1024**2,wall_cap_s=180,complete_momentum_max=1e-11,divergence_max=1e-8,flux_max=1e-9,discrete_momentum_max=1e-9,discrete_energy_max=1e-9)
 save(directory/'contract.json',contract);result=dict(status='failed',processes={},native_operator_changed=False,physical_accuracy_certified=False)
 try:
  command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY','-I'+str(frozen/'include'),str(frozen/'tests/cfd_obstacle3d_pressure_trace_readback.c')]
  command += [str(frozen/'src/app'/q) for q in ('cfd_obstacle3d_pressure_trace.c','cfd_obstacle3d_reconstruction.c','cfd_cartesian3d.c','cfd_sparse_mg.c','cfd_memory.c')]
  command += ['-lm','-o',str(directory/'probe')]
  result['processes']['compile']=compile_probe(command,directory,'compile',60,1024**3)
  result['processes']['readback']=execute([str(directory/'probe'),str(c['n']),str(c['length']),repr(c['pressure_drop_pa']),str(binary),str(directory/'control.json')],directory,'readback',180,1536*1024**2)
  observed=json.loads((directory/'control.json').read_text())
  for key in ('pressure_force_n','viscous_force_n','candidate_pressure_force_n'):np.testing.assert_allclose(observed[key],c[key],rtol=0,atol=1e-13)
  shape=(round(c['length']*c['n']/2),c['n'],c['n'])
  with binary.open('rb') as f:require(struct.unpack('=3i',f.read(12))==shape,'Header identity')
  require(binary.stat().st_size==12+(4*np.prod(shape)+shape[1]*shape[2])*8,'Complete native field length')
  values=np.memmap(binary,dtype='=f8',mode='r',offset=12,shape=(shape[2],shape[1],shape[0],4))
  h=2/c['n'];lo=[round((c['center_x']-.5)/h),round(.5/h),round(.5/h)];hi=[round((c['center_x']+.5)/h),round(1.5/h),round(1.5/h)]
  means=[]
  for side in (0,1):means.append([float(values[lo[2]:hi[2],lo[1]:hi[1],lo[0]-1-d if side==0 else hi[0]+d,3].mean()) for d in range(4)])
  independent=float(np.dot([25,-23,13,-3],np.array(means[0])-means[1])/12)
  require(abs(independent-observed['candidate_pressure_force_n'][0])<1e-13,'Independent binary pressure integration')
  result['control']=observed;result['independent_candidate_pressure_force_x_n']=independent
  result['source_sha256']=sources
  for q,h in sources.items():require(sha(ROOT/q)==h==sha(frozen/q),'Source drift')
  require(sha(receipt)==contract['input_receipt_sha256'] and sha(binary)==contract['input_binary_sha256'],'Input drift')
  result['status']='passed_native_pressure_trace_api_readback'
 except Exception as error:result['failure']=str(error)
 result['artifact_sha256']={str(p.relative_to(directory)):sha(p) for p in sorted(directory.rglob('*')) if p.is_file()}
 save(directory/'receipt.json',portable_paths(result,directory));seal_bundle(directory);print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'),failure=result.get('failure'))))
 return 0 if result['status'].startswith('passed') else 1
if __name__=='__main__':raise SystemExit(main())
