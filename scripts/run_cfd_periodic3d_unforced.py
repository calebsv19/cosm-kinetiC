"""Freeze and repeat the independent unforced periodic 3D accuracy regression.

Local source proof only. Each unique name retains a bounded success/failure
receipt, exact input hashes and compiler/run logs. No wall/obstacle qualification.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
from run_cfd_native_accuracy_regression import execute, require, save, sha

ROOT = Path(__file__).resolve().parents[1]
FILES = ('scripts/run_cfd_periodic3d_unforced.py',
         'scripts/run_cfd_native_accuracy_regression.py',
         'scripts/assess_cfd_periodic3d_unforced.py',
         'tests/cfd_periodic3d_unforced_test.c',
         'src/app/cfd_periodic3d.c', 'src/app/cfd_cartesian3d.c',
         'src/app/cfd_duct3d.c', 'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c',
         'include/app/cfd_periodic3d.h', 'include/app/cfd_cartesian3d.h',
         'include/app/cfd_duct3d.h', 'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name), 'Run identity')
    directory = ROOT/'build/c3d-unforced-periodic/runs'/args.name
    directory.mkdir(parents=True, exist_ok=False)
    frozen = directory/'source'
    sources = {q: sha(ROOT/q) for q in FILES}
    for q,h in sources.items():
        p = frozen/q; p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes((ROOT/q).read_bytes()); require(sha(p)==h, 'Source freeze '+q)
    save(directory/'contract.json', dict(source_sha256=sources, compiler_wall_cap_s=60,
        run_wall_cap_s=600, sampled_rss_cap_bytes=512*1024**2,
        owner_cap_bytes=256*1024**2, velocity_finest_max=.01,
        pressure_finest_max=.05, refinement_ratio_min=3.,
        complete_momentum_max=1e-11, divergence_max=1e-8,
        performance_gate_applied=False, wall_open_obstacle_certification=False))
    result = dict(status='failed', source_sha256=sources, processes={},
                  wall_open_obstacle_certification=False)
    try:
        sources_c = [str(frozen/q) for q in FILES if q.endswith('.c')]
        for label, flags in [('normal',['-O2']),('sanitize',['-O1','-g',
               '-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
            executable = directory/label
            result['processes']['compile_'+label] = execute(
                ['clang','-std=c11','-Wall','-Wextra','-Werror',*flags,
                 '-I'+str(frozen/'include'),*sources_c,'-lm','-o',str(executable)],
                directory,'compile_'+label,60,512*1024**2)
            result['processes'][label] = execute(
                [str(executable)]+(['small'] if label=='sanitize' else []),
                directory,label,600,512*1024**2)
        spec=importlib.util.spec_from_file_location('frozen_unforced_assessor',
            frozen/'scripts/assess_cfd_periodic3d_unforced.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        result['assessment']=module.assess(directory/'normal.stdout')
        result['sanitizer']=json.loads((directory/'sanitize.stdout').read_text())
        require(result['sanitizer']['maximum_true_residual']<=1e-11 and
                result['sanitizer']['forcing_power_w']==0, 'Sanitizer numerical controls')
        for q,h in sources.items():
            require(sha(ROOT/q)==h and sha(frozen/q)==h,'Final source identity '+q)
        result['status']='passed_frozen_unforced_periodic_regression'
    except Exception as error:
        result['failure']=str(error)
        raise
    finally:
        result['artifact_sha256']={str(p.relative_to(directory)):sha(p)
            for p in directory.rglob('*') if p.is_file()}
        save(directory/'receipt.json',result)
    print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'))))

if __name__=='__main__':
    main()
