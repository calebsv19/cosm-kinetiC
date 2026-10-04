"""Frozen pressure-driven native cube field with current and candidate pressure loads."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
from run_cfd_native_accuracy_regression import execute, require, save, sha
ROOT=Path(__file__).resolve().parents[1]
FILES=('scripts/run_cfd_native_cube_pressure.py','scripts/run_cfd_native_accuracy_regression.py',
    'tests/cfd_obstacle3d_cube_pressure_probe.c','tests/cfd_obstacle3d_wall_pressure_candidate.h',
    'src/app/cfd_obstacle3d.c','src/app/cfd_obstacle3d_mixed.c','src/app/cfd_obstacle3d_reconstruction.c',
    'src/app/cfd_cartesian3d.c','src/app/cfd_sparse_mg.c','src/app/cfd_memory.c',
    'include/app/cfd_obstacle3d.h','include/app/cfd_mixed3d.h','include/app/cfd_cartesian3d.h',
    'include/app/cfd_sparse_mg.h','include/app/cfd_memory.h')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--name',required=True)
    p.add_argument('--n',type=int,choices=(16,32,64,80),required=True);p.add_argument('--length',type=int,choices=(4,8),required=True)
    args=p.parse_args();require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',args.name),'Invalid run name')
    require(args.length*args.n**3//2<=1048576,'Native Cartesian cell admission')
    source={q:sha(ROOT/q) for q in FILES};bundle=hashlib.sha256(json.dumps(source,sort_keys=True).encode()).hexdigest()
    directory=ROOT/'build/c3d-native-cube-pressure/runs'/bundle/args.name;directory.mkdir(parents=True,exist_ok=False)
    frozen=directory/'source'
    for q,h in source.items():
        file=frozen/q;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes((ROOT/q).read_bytes());require(sha(file)==h,'Freeze drift')
    contract=dict(source_sha256=source,n=args.n,length_m=args.length,body_center_x_m=args.length/2,
        body_size_m=[1,1,1],cross_section_m=[2,2],density_kg_m3=1,viscosity_pa_s=.1,flow_m3_s=.008,
        owned_cap_bytes=1024**3,rss_cap_bytes=1536*1024**2,fixture_wall_cap_s=600,supervisor_wall_cap_s=630,
        momentum_residual_max=1e-11,divergence_max=1e-8,flux_error_max=1e-8,discrete_energy_max=1e-9,
        physical_energy_max=.03,native_separate_force_reference_gate=.05,native_scalar_reference_gate=.03,
        independent_reference_raw_equilibrium_gate=.01,native_grid_cell_cap=1048576,
        native_default_adopted=False,physical_accuracy_certified=False,
        scope='same pressure-driven unit cube and original equations; pressure candidate is an extra observer only')
    save(directory/'contract.json',contract)
    result=dict(schema='physics_sim_native_cube_pressure_receipt_v1',status='failed',processes={},
        native_operator_changed=False,native_default_adopted=False,physical_accuracy_certified=False,persistent_goal_complete=False)
    try:
        result['compiler']=subprocess.check_output(['clang','--version'],text=True,timeout=10).splitlines()[0]
        command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY','-I'+str(frozen/'include'),
                 str(frozen/'tests/cfd_obstacle3d_cube_pressure_probe.c')]
        command += [str(frozen/'src/app'/q) for q in ('cfd_obstacle3d_mixed.c','cfd_obstacle3d_reconstruction.c','cfd_cartesian3d.c','cfd_sparse_mg.c','cfd_memory.c')]
        command += ['-lm','-o',str(directory/'probe')]
        result['processes']['compile']=execute(command,directory,'compile',60,1024**3)
        print(f'Running unchanged pressure-driven cube L{args.length}/n{args.n}',flush=True)
        result['processes']['field']=execute([str(directory/'probe'),str(args.n),str(args.length),str(args.length/2),str(directory/'field.bin')],directory,'field',630,1536*1024**2)
        row=json.loads((directory/'field.stdout').read_text())
        require(row['n']==args.n and row['length']==args.length and row['center_x']==args.length/2,'Case identity')
        require(all(math.isfinite(row[k]) for k in ('relative_residual','divergence','flux_error','physical_energy_imbalance','discrete_energy_imbalance')),'Finite diagnostic')
        result['control']=row
        result['numerical_gates_passed']=row['relative_residual']<=1e-11 and row['divergence']<1e-8 and row['flux_error']<1e-8 and row['discrete_energy_imbalance']<1e-9
        result['physical_energy_gate_passed']=row['physical_energy_imbalance']<=.03
        require(result['numerical_gates_passed'],'Original numerical gates')
        require(row['numerical_peak_bytes']<1024**3 and row['wall_s']<600,'Owned resources')
        for q,h in source.items():require(sha(ROOT/q)==h==sha(frozen/q),'Source drift: '+q)
        result['status']='completed_native_cube_pressure_comparison'
    except Exception as error:result['failure']=str(error)
    result['artifact_sha256']={str(q.relative_to(directory)):sha(q) for q in sorted(directory.rglob('*')) if q.is_file()}
    save(directory/'receipt.json',result);print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'),sha256=sha(directory/'receipt.json'))),flush=True)
    return 0 if result['status'].startswith('completed') else 1
if __name__=='__main__':raise SystemExit(main())
