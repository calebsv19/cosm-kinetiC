"""One separately bounded n128 cube-wall accuracy field; original native equations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import numpy as np
from run_cfd_native_accuracy_regression import execute, require, save, sha
from run_cfd_native_wall_shear import independently_verify_integrals
ROOT = Path(__file__).resolve().parents[1]
PREVIOUS = ROOT/'build/c3d-wall-pressure/runs/483fc9b3f637630c9fe75433aa15dbbaec94142dc5f19d66ed710753e165ca96/wall-pressure-01/receipt.json'
FILES = ('scripts/run_cfd_native_wall_shear_fine.py', 'scripts/run_cfd_native_accuracy_regression.py',
         'scripts/run_cfd_native_wall_shear.py', 'tests/cfd_obstacle3d_wall_shear_fine_probe.c',
         'tests/cfd_obstacle3d_wall_shear_probe.c', 'tests/cfd_obstacle3d_wall_shear_integral_probe.c',
         'tests/cfd_obstacle3d_wall_shear_candidate.h', 'tests/cfd_obstacle3d_wall_pressure_candidate.h',
         'src/app/cfd_obstacle3d_mixed.c', 'src/app/cfd_obstacle3d.c', 'src/app/cfd_obstacle3d_reconstruction.c',
         'src/app/cfd_cartesian3d.c', 'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c',
         'include/app/cfd_obstacle3d.h', 'include/app/cfd_mixed3d.h', 'include/app/cfd_cartesian3d.h',
         'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h')


def main():
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('--name', required=True);args=p.parse_args()
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name), 'Invalid run name')
    sources = {q:sha(ROOT/q) for q in FILES};bundle=hashlib.sha256(json.dumps(sources,sort_keys=True).encode()).hexdigest()
    directory=ROOT/'build/c3d-wall-shear-fine/runs'/bundle/args.name;directory.mkdir(parents=True,exist_ok=False)
    frozen=directory/'source'
    for q,h in sources.items():
        file=frozen/q;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes((ROOT/q).read_bytes());require(sha(file)==h,'Freeze drift')
    previous=json.loads(PREVIOUS.read_text());require(previous['status']=='completed_nonzero_cube_wall_shear_investigation','Predecessor terminal success')
    contract=dict(source_sha256=sources,n=128,predecessor_receipt=str(PREVIOUS),predecessor_receipt_sha256=sha(PREVIOUS),
        owned_cap_bytes=4*1024**3,rss_cap_bytes=6*1024**3,fixture_wall_cap_s=1800,supervisor_wall_cap_s=1830,
        momentum_residual_max=1e-11,maximum_divergence_max=1e-8,velocity_error_target=.01,pressure_error_target=.003,
        maximum_field_refinement_error_ratio=.4,known_answer_total_force_error_max=.01,
        original_cube_qualification_gates_changed=False,native_default_adopted=False)
    save(directory/'contract.json',contract)
    result=dict(schema='physics_sim_native_wall_shear_fine_receipt_v1',status='failed',processes={},
        native_operator_changed=False,native_default_adopted=False,physical_cube_force_qualification=False,persistent_goal_complete=False)
    try:
        result['compiler']=subprocess.check_output(['clang','--version'],text=True,timeout=10).splitlines()[0]
        for label,fixture in (('field','cfd_obstacle3d_wall_shear_fine_probe.c'),('integrals','cfd_obstacle3d_wall_shear_integral_probe.c')):
            command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY','-I'+str(frozen/'include'),str(frozen/'tests'/fixture)]
            command += [str(frozen/'src/app'/q) for q in ('cfd_cartesian3d.c','cfd_sparse_mg.c','cfd_memory.c')]
            command += ['-lm','-o',str(directory/label)]
            result['processes']['compile_'+label]=execute(command,directory,'compile_'+label,60,1024**3)
        result['processes']['integrals']=execute([str(directory/'integrals')],directory,'integrals',30,1024**3)
        result['independent_forcing']=independently_verify_integrals(json.loads((directory/'integrals.stdout').read_text()))
        print('Running one frozen n128 cube-wall accuracy field; owned4GiB/RSS6GiB/wall1800s',flush=True)
        result['processes']['n128']=execute([str(directory/'field'),'128'],directory,'n128',1830,6*1024**3)
        row=json.loads((directory/'n128.stdout').read_text());require(row['n']==128,'Resolution identity')
        require(row['momentum_residual']<1e-11 and row['maximum_divergence']<1e-8,'Original numerical gates')
        require(row['wall_s']<1800 and row['peak_owned_bytes']<4*1024**3,'Owned bounds')
        exact=np.array(result['independent_forcing']['analytic_viscous_force_n']);np.testing.assert_allclose(row['analytic_viscous_force_n'],exact,rtol=0,atol=1e-14)
        scale=float(np.linalg.norm(exact));row['force_errors']={}
        for mode in ('solved','prescribed'):
            for candidate in (False,True):
                key=mode+('_candidate' if candidate else '')+'_viscous_force_n'
                row['force_errors'][key]=float(np.linalg.norm(np.array(row[key])-exact)/scale)
                key=mode+('_candidate' if candidate else '')+'_pressure_force_n'
                row['force_errors'][key]=float(np.linalg.norm(row[key])/scale)
            for candidate in (False,True):
                pressure_key=mode+('_candidate' if candidate else '')+'_pressure_force_n'
                # The viscosity candidate was rejected; preserve the current viscosity trace.
                total=np.array(row[pressure_key])+np.array(row[mode+'_viscous_force_n'])
                row['force_errors'][mode+('_candidate_pressure' if candidate else '')+'_total_force_n']=float(np.linalg.norm(total-exact)/scale)
        require(all(np.isfinite(v) for v in row['force_errors'].values()),'Finite forces')
        result['control']=row;coarse=previous['controls']['64']
        result['field_error_ratios_to_n64']={key:row[key]/coarse[key] for key in ('velocity_relative_error','pressure_relative_error')}
        result['field_targets_passed']=row['velocity_relative_error']<=.01 and row['pressure_relative_error']<=.003
        result['field_refinement_passed']=all(ratio<=.4 for ratio in result['field_error_ratios_to_n64'].values())
        result['current_total_force_target_passed']=row['force_errors']['solved_total_force_n']<=.01
        result['candidate_pressure_total_force_target_passed']=row['force_errors']['solved_candidate_pressure_total_force_n']<=.01
        result['known_answer_wall_accuracy_targets_passed']=result['field_targets_passed'] and result['field_refinement_passed'] and result['current_total_force_target_passed']
        for q,h in sources.items():require(sha(ROOT/q)==h==sha(frozen/q),'Source drift: '+q)
        require(sha(PREVIOUS)==contract['predecessor_receipt_sha256'],'Predecessor drift')
        result['status']='completed_fine_native_wall_shear_known_answer'
    except Exception as error:result['failure']=str(error)
    result['source_sha256']=sources
    result['artifact_sha256']={str(q.relative_to(directory)):sha(q) for q in sorted(directory.rglob('*')) if q.is_file()}
    save(directory/'receipt.json',result)
    print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'),sha256=sha(directory/'receipt.json'))),flush=True)
    return 0 if result['status'].startswith('completed') else 1


if __name__=='__main__':raise SystemExit(main())
