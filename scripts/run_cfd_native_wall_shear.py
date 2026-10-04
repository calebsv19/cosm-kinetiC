"""Frozen native cube-wall manufactured Stokes investigation; no default changes."""
import argparse
import importlib.util
import json
import hashlib
import math
from pathlib import Path
import re
import subprocess
import numpy as np
from run_cfd_native_accuracy_regression import execute, require, save, sha

ROOT = Path(__file__).resolve().parents[1]
FILES = (
    'scripts/run_cfd_native_wall_shear.py', 'scripts/run_cfd_native_accuracy_regression.py',
    'tests/cfd_obstacle3d_wall_shear_probe.c',
    'tests/cfd_obstacle3d_wall_shear_integral_probe.c',
    'tests/cfd_obstacle3d_wall_shear_candidate.h',
    'src/app/cfd_obstacle3d_mixed.c', 'src/app/cfd_obstacle3d.c',
    'src/app/cfd_obstacle3d_reconstruction.c', 'src/app/cfd_cartesian3d.c',
    'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c',
    'include/app/cfd_obstacle3d.h', 'include/app/cfd_mixed3d.h',
    'include/app/cfd_cartesian3d.h', 'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h',
)
LOW, HIGH = np.array([.25, .625, .625]), np.array([1.5, 1.375, 1.375])


def independent_derivative(t, axis, derivative):
    """Unexpanded Leibniz evaluation, independent of the C coefficient arrays."""
    left, right, factor = (4, 2, 45.5625) if axis == 0 else (4, 4, 256.)
    value = 0.
    for j in range(derivative+1):
        k = derivative-j
        if j <= left and k <= right:
            value += (math.comb(derivative, j)*math.factorial(left)/math.factorial(left-j)*
                      t**(left-j)*(-1)**k*math.factorial(right)/math.factorial(right-k)*
                      (1-t)**(right-k))
    return factor*value


def independently_verify_integrals(samples):
    require(len(samples) == 84, 'Integral count')
    nodes, weights = np.polynomial.legendre.leggauss(6)
    maximum = 0.
    for axis, a, b, derivative, value in samples:
        require(axis in (0, 1, 2) and derivative in (0, 1, 2, 3) and b > a,
                'Integral identity')
        lower, upper = max(a, LOW[axis]), min(b, HIGH[axis])
        expected = 0.
        if upper > lower:
            points = (upper+lower)/2 + (upper-lower)*nodes/2
            t = (points-LOW[axis])/(HIGH[axis]-LOW[axis])
            expected = float(np.dot(weights, independent_derivative(t, axis, derivative)) /
                             (HIGH[axis]-LOW[axis])**derivative * (upper-lower)/(2*(b-a)))
        require(math.isfinite(value), 'Nonfinite integral')
        maximum = max(maximum, abs(value-expected))
    require(maximum < 2e-8, 'Independent forcing integral agreement')
    # Nonzero second derivative is the physical one-sided cube-wall derivative.
    edge_second = float(independent_derivative(1., 0, 2)/(HIGH[0]-LOW[0])**2)
    area_integrals = [(HIGH[a]-LOW[a])*float(np.dot(weights, independent_derivative((nodes+1)/2, a, 0)))/2
                      for a in (1, 2)]
    exact = .1*.0001*edge_second*math.prod(area_integrals)
    require(edge_second > 0 and exact > 0, 'Wall shear is nonzero')
    # Two interval means with zero wall trace reproduce a quadratic derivative.
    for a, b in ((1.2, -.7), (-.3, 2.1), (0., 1.)):
        for h in (.25, .125, .03125):
            near = a*h/2+b*h*h/3
            far = 3*a*h/2+7*b*h*h/3
            require(abs((7*near-far)/(2*h)-a) < 1e-13, 'Quadratic interval derivative')
    return dict(samples=len(samples), maximum_error=maximum,
                one_sided_wall_second_derivative=edge_second, analytic_viscous_force_n=[0., exact, 0.])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name), 'Invalid run name')
    sources = {q: sha(ROOT/q) for q in FILES}
    bundle = hashlib.sha256(json.dumps(sources, sort_keys=True).encode()).hexdigest()
    directory = ROOT/'build/c3d-wall-shear/runs'/bundle/args.name
    directory.mkdir(parents=True, exist_ok=False)
    frozen = directory/'source'
    for q, h in sources.items():
        p = frozen/q; p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes((ROOT/q).read_bytes()); require(sha(p) == h, 'Freeze drift: '+q)
    contract = dict(source_sha256=sources, source_bundle_sha256=bundle, resolutions=[8, 16, 32, 64],
                    low_m=LOW.tolist(), high_m=HIGH.tolist(), cube_lo_m=[1.5, .5, .5],
                    cube_hi_m=[2.5, 1.5, 1.5], viscosity_pa_s=.1, streamfunction_amplitude=.0001,
                    pressure_amplitude_pa=.001, exact_cube_pressure_force_n=[0., 0., 0.],
                    momentum_residual_max=1e-11, maximum_divergence_max=1e-8,
                    owned_cap_bytes=1024**3, fixture_wall_cap_s=600,
                    supervisor_rss_cap_bytes=1536*1024**2, supervisor_wall_cap_s=630,
                    independent_integral_error_max=2e-8, fine_velocity_error_target=.01,
                    fine_pressure_error_target=.003, candidate_prescribed_force_error_max=.01,
                    candidate_solved_force_error_max=.05, candidate_error_ratio_max=.5,
                    original_cube_qualification_gates_changed=False, native_default_adopted=False,
                    shared_reuse='existing native fixtures, observer and Python supervisor; app-specific numerical verification, no new shared runtime semantics')
    save(directory/'contract.json', contract)
    result = dict(schema='physics_sim_native_wall_shear_receipt_v1', status='failed',
                  source_bundle_sha256=bundle, controls={}, processes={},
                  native_operator_changed=False, native_default_adopted=False,
                  physical_cube_force_qualification=False, persistent_goal_complete=False)
    try:
        result['compiler'] = subprocess.check_output(['clang', '--version'], text=True, timeout=10).splitlines()[0]
        for label, source in (('field', 'cfd_obstacle3d_wall_shear_probe.c'),
                              ('integrals', 'cfd_obstacle3d_wall_shear_integral_probe.c')):
            command = ['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DCFD_MIXED3D_VERIFY',
                       '-I'+str(frozen/'include'), str(frozen/'tests'/source)]
            command += [str(frozen/'src/app'/q) for q in ('cfd_cartesian3d.c', 'cfd_sparse_mg.c', 'cfd_memory.c')]
            command += ['-lm', '-o', str(directory/label)]
            result['processes']['compile_'+label] = execute(command, directory, 'compile_'+label, 60, 1536*1024**2)
        result['processes']['integrals'] = execute([str(directory/'integrals')], directory, 'integrals', 30, 1536*1024**2)
        result['independent_forcing'] = independently_verify_integrals(json.loads((directory/'integrals.stdout').read_text()))
        for n in contract['resolutions']:
            print('Running frozen nonzero cube-wall shear: n'+str(n), flush=True)
            tag = 'n'+str(n)
            result['processes'][tag] = execute([str(directory/'field'), str(n)], directory, tag, 630, 1536*1024**2)
            row = json.loads((directory/(tag+'.stdout')).read_text())
            require(row['n'] == n and row['momentum_residual'] < 1e-11 and row['maximum_divergence'] < 1e-8,
                    tag+': original numerical acceptance')
            require(row['wall_s'] < 600 and row['peak_owned_bytes'] < 1024**3, tag+': owned resources')
            exact = np.array(result['independent_forcing']['analytic_viscous_force_n'])
            np.testing.assert_allclose(row['analytic_viscous_force_n'], exact, rtol=0, atol=1e-14)
            scale = float(np.linalg.norm(exact))
            row['force_errors'] = {}
            for mode in ('solved', 'prescribed'):
                for candidate in (False, True):
                    key = mode+('_candidate' if candidate else '')+'_viscous_force_n'
                    row['force_errors'][key] = float(np.linalg.norm(np.array(row[key])-exact)/scale)
                key = mode+'_pressure_force_n'
                row['force_errors'][key] = float(np.linalg.norm(row[key])/scale)
            require(all(math.isfinite(row[key]) and row[key] > 0 for key in ('velocity_relative_error', 'pressure_relative_error')),
                    tag+': invalid physical error')
            require(all(math.isfinite(value) for value in row['force_errors'].values()), tag+': nonfinite force')
            result['controls'][str(n)] = row
            print(json.dumps(dict(n=n, velocity_error=row['velocity_relative_error'], pressure_error=row['pressure_relative_error'],
                                  force_errors=row['force_errors'])), flush=True)
        result['observed_orders'] = {}
        for n in (16, 32, 64):
            result['observed_orders'][str(n)] = {key: math.log2(result['controls'][str(n//2)][key]/result['controls'][str(n)][key])
                                                for key in ('velocity_relative_error', 'pressure_relative_error')}
        fine = result['controls']['64']; errors = fine['force_errors']
        result['fine_field_targets_passed'] = fine['velocity_relative_error'] <= .01 and fine['pressure_relative_error'] <= .003
        result['candidate_prescribed_force_passed'] = errors['prescribed_candidate_viscous_force_n'] <= .01
        result['candidate_solved_force_passed'] = errors['solved_candidate_viscous_force_n'] <= .05
        result['candidate_improvement_passed'] = all(errors[mode+'_candidate_viscous_force_n'] <= .5*errors[mode+'_viscous_force_n']
                                                   for mode in ('solved', 'prescribed'))
        result['candidate_permitted_for_next_diagnostic'] = (result['candidate_prescribed_force_passed'] and
            result['candidate_solved_force_passed'] and result['candidate_improvement_passed'])
        for q, h in sources.items():
            require(sha(ROOT/q) == h == sha(frozen/q), 'Source drift: '+q)
        result['status'] = 'completed_nonzero_cube_wall_shear_investigation'
    except Exception as error:
        result['failure'] = str(error)
    result['artifact_sha256'] = {str(p.relative_to(directory)): sha(p) for p in sorted(directory.rglob('*')) if p.is_file()}
    save(directory/'receipt.json', result)
    print(json.dumps(dict(status=result['status'], receipt=str(directory/'receipt.json'), sha256=sha(directory/'receipt.json'))), flush=True)
    return 0 if result['status'] == 'completed_nonzero_cube_wall_shear_investigation' else 1


if __name__ == '__main__':
    raise SystemExit(main())
