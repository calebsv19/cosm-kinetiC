"""Contained frozen-field diagnostic build; no shared/Thermal linkage."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument("--edge-candidate",action="store_true");p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
gas=[r/f'src/app/{x}.c' for x in ['cfd_open_atmosphere3d_box','cfd_open_atmosphere3d_box_preconditioner','cfd_open_atmosphere3d_lateral','cfd_open_atmosphere3d_wind','cfd_open_atmosphere3d_vent','cfd_open_atmosphere3d_vent_pressure','cfd_open_atmosphere3d_vent_balance','cfd_passive3d','cfd_cartesian3d','cfd_sparse_mg','cfd_memory']]+[r/'src/tools/physics_sim_heated_face.c']
driver=r/'src/tools/physics_sim_thermal_edge_probe.c';ref=r/'src/tools/physics_sim_thermal_edge_reference.c'
headers=[r/'src/app/cfd_scalar_edge_candidate.h']+list((r/'include/app').glob('*.h'))+[r/'src/tools/physics_sim_heated_face.h']
inputs=[r/'tests/cfd_scalar_edge_candidate_test.c']+gas+headers+[driver,ref,r/'src/tools/physics_sim_thermal_columns_reference.c',r/'src/app/cfd_open_atmosphere3d.c',Path(__file__)]+[r/'tests'/x for x in ['cfd_heated_columns_test.c','cfd_heated_face_test.c','cfd_heated_face_support_test.c']]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
identity={str(x):sha(x) for x in inputs}
flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off','-DPHYSICS_SIM_THIN_TRANSPORT_QUALIFIER=1','-I'+str(r/'include'),'-I'+str(r/'src/tools')]
if a.edge_candidate:flags+=['-DPHYSICS_SIM_OPEN_EDGE_CANDIDATE=1']
for name,extra in [('probe',[]),('probe-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 subprocess.run(['clang',*flags,*extra,*map(str,gas),str(driver),'-lm','-o',str(out/name)],check=True)
subprocess.run(['clang',*flags,str(ref),'-lm','-o',str(out/'reference')],check=True)
for name,source in [('columns-test','cfd_heated_columns_test.c'),('face-test','cfd_heated_face_test.c'),('support-test','cfd_heated_face_support_test.c')]:
 subprocess.run(['clang',*flags,*map(str,gas),str(r/'src/app/cfd_open_atmosphere3d.c'),str(r/'tests'/source),'-lm','-o',str(out/name)],check=True)
if a.edge_candidate:
 for name,extra in [('candidate-test',[]),('candidate-test-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
  subprocess.run(['clang',*flags,*extra,*map(str,gas),str(r/'tests/cfd_scalar_edge_candidate_test.c'),'-lm','-o',str(out/name)],check=True)
assert all(sha(Path(k))==v for k,v in identity.items())
(out/'identity.json').write_text(json.dumps(dict(inputs=identity,flags=flags,binaries={p.name:sha(p) for p in out.iterdir() if p.is_file()}),indent=2)+'\n')
