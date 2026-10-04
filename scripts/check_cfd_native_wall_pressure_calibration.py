"""Compile and seal actual C pressure-candidate polynomial/fallback/cusp controls."""
import json
from pathlib import Path
from run_cfd_native_accuracy_regression import execute, require, save, sha
from run_cfd_native_wall_pressure import FILES
ROOT=Path(__file__).resolve().parents[1];DEST=ROOT/'build/c3d-wall-pressure-calibration'
def main():
    DEST.mkdir(exist_ok=False)
    files=set(FILES)|{'scripts/run_cfd_native_wall_pressure.py','scripts/check_cfd_native_wall_pressure_calibration.py',
                      'tests/cfd_obstacle3d_wall_pressure_calibration.c'}
    hashes={q:sha(ROOT/q) for q in sorted(files)};frozen=DEST/'source'
    for q,h in hashes.items():
        p=frozen/q;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((ROOT/q).read_bytes());require(sha(p)==h,'Freeze drift')
    save(DEST/'contract.json',dict(source_sha256=hashes,owned_cap_bytes=512*1024**2,process_rss_cap_bytes=768*1024**2,
         wall_cap_s=120,patch_error_max_n=1e-11,gauge_shift_error_max_n=1e-11,cusp_error_ratio_max_difference=1e-9,
         native_default_adopted=False))
    result=dict(status='failed',processes={},native_default_adopted=False,physical_cube_force_qualification=False)
    try:
        command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY','-I'+str(frozen/'include'),
                 str(frozen/'tests/cfd_obstacle3d_wall_pressure_calibration.c')]
        command += [str(frozen/'src/app'/q) for q in ('cfd_cartesian3d.c','cfd_sparse_mg.c','cfd_memory.c')]
        command += ['-lm','-o',str(DEST/'probe')]
        result['processes']['compile']=execute(command,DEST,'compile',60,768*1024**2)
        result['processes']['controls']=execute([str(DEST/'probe')],DEST,'controls',120,768*1024**2)
        row=json.loads((DEST/'controls.stdout').read_text());require(row['maximum_patch_error_n']<1e-11 and row['gauge_shift_error_n']<1e-11,'Patch/gauge calibration')
        require(all(q>0 for q in row['fallback_degree_samples']),'Every fallback exercised')
        result['control']=row
        for q,h in hashes.items():require(sha(ROOT/q)==h==sha(frozen/q),'Source drift')
        result['status']='passed_actual_c_pressure_candidate_calibration'
        result['scope']='individual physical patches; cubic/quadratic/linear exactness with four/three/two available intervals, pressure offset, and square-root counterexample retaining half order'
    except Exception as error:result['failure']=str(error)
    result['source_sha256']=hashes
    result['artifact_sha256']={str(p.relative_to(DEST)):sha(p) for p in sorted(DEST.rglob('*')) if p.is_file()}
    save(DEST/'checkpoint-audit.json',result);print(json.dumps(dict(status=result['status'],sha256=sha(DEST/'checkpoint-audit.json'),control=result.get('control'),failure=result.get('failure'))))
    return 0 if result['status'].startswith('passed') else 1
if __name__=='__main__':raise SystemExit(main())
