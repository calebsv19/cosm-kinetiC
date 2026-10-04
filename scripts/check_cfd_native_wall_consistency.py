"""Bound and freeze local polynomial consistency checks on the native cube wall."""
import hashlib
import json
from pathlib import Path
import numpy as np
from run_cfd_native_accuracy_regression import execute, require, save, sha
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'build/c3d-wall-consistency'


def main():
    DEST.mkdir(exist_ok=False)
    files = ['scripts/check_cfd_native_wall_consistency.py', 'scripts/run_cfd_native_accuracy_regression.py',
             'tests/cfd_obstacle3d_wall_consistency_probe.c', 'src/app/cfd_obstacle3d_mixed.c',
             'src/app/cfd_cartesian3d.c', 'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c',
             'include/app/cfd_obstacle3d.h', 'include/app/cfd_mixed3d.h',
             'include/app/cfd_cartesian3d.h', 'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h']
    hashes = {q: sha(ROOT/q) for q in files}
    frozen = DEST/'source'
    for q, h in hashes.items():
        p = frozen/q; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes((ROOT/q).read_bytes())
        require(sha(p) == h, 'Freeze drift')
    save(DEST/'contract.json', dict(source_sha256=hashes, resolutions=[8, 16, 32],
         owned_cap_bytes=512*1024**2, process_rss_cap_bytes=768*1024**2,
         wall_cap_s=120, maximum_prediction_error=1e-12, native_operator_changed=False,
         scope='six local divergence-free tangential polynomial fields per grid; no global flow solve'))
    result = dict(status='failed', processes={}, controls={}, native_operator_changed=False,
                  physical_cube_force_qualification=False)
    try:
        command = ['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DCFD_MIXED3D_VERIFY',
                   '-I'+str(frozen/'include'), str(frozen/'tests/cfd_obstacle3d_wall_consistency_probe.c')]
        command += [str(frozen/'src/app'/q) for q in ('cfd_cartesian3d.c', 'cfd_sparse_mg.c', 'cfd_memory.c')]
        command += ['-lm', '-o', str(DEST/'probe')]
        result['processes']['compile'] = execute(command, DEST, 'compile', 60, 768*1024**2)
        for n in (8, 16, 32):
            name = 'n'+str(n)
            result['processes'][name] = execute([str(DEST/'probe'), str(n)], DEST, name, 120, 768*1024**2)
            row = json.loads((DEST/(name+'.stdout')).read_text())
            require(row['n'] == n and row['local_polynomial_cases'] == 6, 'Case identity')
            np.testing.assert_allclose(row['interval_mean_wall_density_defect'], .1*2/3, rtol=0, atol=1e-12)
            np.testing.assert_allclose(row['point_wall_density_defect'], .05, rtol=0, atol=1e-12)
            require(max(row[k] for k in ('maximum_prediction_error', 'maximum_interior_error', 'maximum_linear_error')) < 1e-12,
                    'Polynomial consistency prediction')
            require(row['peak_owned_bytes'] < 512*1024**2, 'Owned cap')
            result['controls'][str(n)] = row
        for q, h in hashes.items():
            require(sha(ROOT/q) == h == sha(frozen/q), 'Source drift')
        result['status'] = 'verified_native_wall_curvature_consistency_defect'
        result['interpretation'] = ('At the first tangential cube-wall velocity row, quadratic interval-mean velocity gives '
            'a normalized momentum density defect (2/3)*mu*b independent of h; linear wall fields and interior quadratic rows '
            'are exact to roundoff. This local truncation defect does not imply failure of global field convergence or justify '
            'an observer-only correction. Point sampling also leaves a .5*mu*b defect.')
    except Exception as error:
        result['failure'] = str(error)
    result['source_sha256'] = hashes
    result['artifact_sha256'] = {str(p.relative_to(DEST)): sha(p) for p in sorted(DEST.rglob('*')) if p.is_file()}
    save(DEST/'checkpoint-audit.json', result)
    print(json.dumps(dict(status=result['status'], sha256=sha(DEST/'checkpoint-audit.json'), controls=result['controls'])))
    return 0 if result['status'].startswith('verified') else 1


if __name__ == '__main__':
    raise SystemExit(main())
