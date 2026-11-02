"""Fresh, contained native sharp-front diagnostic and actual-operator parity."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
gas=[r/f'src/app/{x}.c' for x in ['cfd_open_atmosphere3d_box','cfd_open_atmosphere3d_box_preconditioner','cfd_open_atmosphere3d_lateral','cfd_open_atmosphere3d_wind','cfd_open_atmosphere3d_vent','cfd_open_atmosphere3d_vent_pressure','cfd_open_atmosphere3d_vent_balance','cfd_passive3d','cfd_cartesian3d','cfd_sparse_mg','cfd_memory']]
probe=r/'src/tools/physics_sim_front_arrival_probe.c';test=r/'tests/cfd_front_arrival_parity_test.c'
inputs=gas+[probe,test,r/'src/tools/physics_sim_front_startup_probe.c',r/'scripts/fire_fluid_front_startup.py',r/'src/tools/physics_sim_scalar_subcell.h',r/'tests/cfd_scalar_subcell_test.c',r/'src/tools/physics_sim_front_continuation_probe.c',r/'scripts/fire_fluid_front_continuation.py',r/'scripts/fire_fluid_front_smooth.py',r/'tests/cfd_scalar_exponential_test.c',r/'tests/cfd_scalar_logquadratic_test.c',r/'src/tools/physics_sim_scalar_logquadratic.h',r/'tests/cfd_scalar_front_stage_test.c',r/'src/tools/physics_sim_scalar_front_target.h',r/'src/tools/physics_sim_front_flux_probe.c',r/'tests/cfd_scalar_fct_test.c',r/'src/tools/physics_sim_scalar_fct.h',r/'src/tools/physics_sim_scalar_exponential.h',r/'src/app/cfd_open_atmosphere3d.c',r/'src/app/cfd_scalar_edge_candidate.h',Path(__file__)]+list((r/'include/app').glob('*.h'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
identity={str(p):sha(p) for p in inputs};flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off','-DPHYSICS_SIM_OPEN_EDGE_CANDIDATE=1','-I'+str(r/'include')]
for suffix,extra in [('',[]),('-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
 subprocess.run(['clang',*flags,*extra,str(r/'src/tools/physics_sim_front_flux_probe.c'),'-lm','-o',str(out/('flux-probe'+suffix))],check=True)
 subprocess.run(['clang',*flags,*extra,str(probe),'-lm','-o',str(out/('probe'+suffix))],check=True)
 subprocess.run(['clang',*flags,*extra,*map(str,gas),str(test),'-lm','-o',str(out/('parity'+suffix))],check=True)
for suffix,extra in [('',[]),('-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
 subprocess.run(['clang',*flags,*extra,str(r/'tests/cfd_scalar_fct_test.c'),'-lm','-o',str(out/('fct-test'+suffix))],check=True)
for suffix,extra in [('',[]),('-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
 for name,source in [('flux-exponential','src/tools/physics_sim_front_flux_probe.c'),('probe-exponential','src/tools/physics_sim_front_arrival_probe.c'),('fct-exponential','tests/cfd_scalar_fct_test.c'),('exponential-test','tests/cfd_scalar_exponential_test.c')]:
  subprocess.run(['clang',*flags,*extra,'-DPHYSICS_SIM_EXPONENTIAL_TARGET=1',str(r/source),'-lm','-o',str(out/(name+suffix))],check=True)
for suffix,extra in [('',[]),('-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
 for name,source in [('flux-front-zero','src/tools/physics_sim_front_flux_probe.c'),('probe-front-zero','src/tools/physics_sim_front_arrival_probe.c'),('fct-front-zero','tests/cfd_scalar_fct_test.c'),('front-stage-test','tests/cfd_scalar_front_stage_test.c')]:
  subprocess.run(['clang',*flags,*extra,'-DPHYSICS_SIM_FRONT_TARGET=1',str(r/source),'-lm','-o',str(out/(name+suffix))],check=True)
for suffix,extra in [('',[]),('-sanitize',['-O1','-g','-fsanitize=address,undefined'])]:
 for name,source in [('startup-front-smooth','src/tools/physics_sim_front_startup_probe.c'),('subcell-test','tests/cfd_scalar_subcell_test.c'),('continuation-front-smooth','src/tools/physics_sim_front_continuation_probe.c'),('flux-front-smooth','src/tools/physics_sim_front_flux_probe.c'),('probe-front-smooth','src/tools/physics_sim_front_arrival_probe.c'),('fct-front-smooth','tests/cfd_scalar_fct_test.c'),('smooth-stage-test','tests/cfd_scalar_front_stage_test.c'),('logquadratic-test','tests/cfd_scalar_logquadratic_test.c')]:
  subprocess.run(['clang',*flags,*extra,'-DPHYSICS_SIM_LOGQUADRATIC_TARGET=1',str(r/source),'-lm','-o',str(out/(name+suffix))],check=True)
assert all(sha(Path(k))==v for k,v in identity.items())
(out/'identity.json').write_text(json.dumps(dict(inputs=identity,flags=flags,binaries={p.name:sha(p) for p in out.iterdir() if p.is_file()}),indent=2)+'\n')
