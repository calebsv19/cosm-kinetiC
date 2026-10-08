"""Repeat the native smooth Stokes accuracy series from a frozen source packet.

Developer source-checkout proof only; does not qualify cube forces or change defaults.
Requires clang and NumPy. Each new run name preserves its own immutable receipt.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import time

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
FILES = (
    'scripts/run_cfd_native_accuracy_regression.py',
    'scripts/cfd_run_support.py', 'scripts/cfd_evidence.py','scripts/check_clean_root.py',
    'scripts/verify_cfd_native_manufactured_stokes.py',
    'tests/cfd_obstacle3d_manufactured_stokes_probe.c',
    'tests/cfd_obstacle3d_manufactured_stokes_fine_probe.c',
    'tests/cfd_obstacle3d_manufactured_integral_probe.c',
    'src/app/cfd_obstacle3d_mixed.c',
    'src/app/cfd_cartesian3d.c', 'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c',
    'include/app/cfd_obstacle3d.h', 'include/app/cfd_mixed3d.h', 'include/app/cfd_cartesian3d.h',
    'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h',
)


from cfd_run_support import compile_probe, execute, require, save, sha
from cfd_evidence import experiment_root, seal_bundle, portable_paths

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--experiment-root', type=Path, default=experiment_root(ROOT))
    parser.add_argument('--name', required=True, help='New unique local run name; never overwritten')
    args = parser.parse_args()
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name), 'Invalid run name')
    sources = {name: sha(ROOT / name) for name in FILES}
    bundle = hashlib.sha256(json.dumps(sources, sort_keys=True).encode()).hexdigest()
    directory = experiment_root(ROOT, args.experiment_root) / 'c3d-native-accuracy-regression/runs' / bundle / args.name
    directory.mkdir(parents=True, exist_ok=False)
    frozen = directory / 'source'
    for name, digest in sources.items():
        path = frozen / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((ROOT / name).read_bytes())
        require(sha(path) == digest, 'Source changed during freeze: ' + name)
    contract = dict(source_sha256=sources, source_bundle_sha256=bundle,
                    resolutions=[8, 16, 32, 64], momentum_residual_max=1e-11,
                    maximum_divergence_max=1e-8, maximum_coarse_error_ratio=.5,
                    maximum_fine_error_ratio=.4, n32_error_max=[.03, .01],
                    n64_error_max=[.01, .003], independent_integral_error_max=2e-8,
                    coarse_owned_cap_bytes=512 * 1024**2, fine_owned_cap_bytes=1024**3,
                    coarse_fixture_wall_cap_s=180, fine_fixture_wall_cap_s=600,
                    supervisor_rss_cap_bytes=1536 * 1024**2,
                    supervisor_coarse_wall_cap_s=210, supervisor_fine_wall_cap_s=630,
                    compiler_wall_cap_s=60, physical_cube_force_qualification=False,
                    performance_gate_applied=False)
    save(directory / 'contract.json', contract)
    result = dict(schema='physics_sim_native_accuracy_regression_v1', status='failed',
                  source_bundle_sha256=bundle, controls={}, processes={},
                  physical_cube_force_qualification=False, native_operator_changed=False,
                  scope='smooth steady Stokes velocity and pressure accuracy; local source proof')
    try:
        result['compiler'] = subprocess.check_output(['clang', '--version'], text=True, timeout=10).splitlines()[0]
        for label, fixture in (
                ('coarse', 'cfd_obstacle3d_manufactured_stokes_probe.c'),
                ('fine', 'cfd_obstacle3d_manufactured_stokes_fine_probe.c'),
                ('integrals', 'cfd_obstacle3d_manufactured_integral_probe.c')):
            command = ['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                       '-DCFD_MIXED3D_VERIFY', '-I' + str(frozen / 'include'),
                       str(frozen / 'tests' / fixture)]
            command += [str(frozen / 'src/app' / source) for source in
                        ('cfd_cartesian3d.c', 'cfd_sparse_mg.c', 'cfd_memory.c')]
            command += ['-lm', '-o', str(directory / label)]
            result['processes']['compile_' + label] = compile_probe(
                command, directory, 'compile_' + label, 60, 1536 * 1024**2)
        for n in contract['resolutions']:
            fine = n == 64
            tag = 'n' + str(n)
            print('Running frozen native known answer: ' + tag, flush=True)
            result['processes'][tag] = execute(
                [str(directory / ('fine' if fine else 'coarse')), str(n)],
                directory, tag, 630 if fine else 210, 1536 * 1024**2)
            row = json.loads((directory / (tag + '.stdout')).read_text())
            require(row['n'] == n, tag + ': resolution identity')
            require(all(math.isfinite(float(value)) for value in row.values()), tag + ': nonfinite output')
            require(row['momentum_residual'] < 1e-11 and row['maximum_divergence'] < 1e-8,
                    tag + ': numerical acceptance')
            require(row['wall_s'] < (600 if fine else 180), tag + ': fixture wall cap')
            require(row['peak_owned_bytes'] < (1024**3 if fine else 512 * 1024**2), tag + ': owned cap')
            require(row['velocity_relative_error'] > 0 and row['pressure_relative_error'] > 0,
                    tag + ': invalid error norm')
            result['controls'][str(n)] = row
        result['observed_orders'] = {}
        for n in (16, 32, 64):
            orders = {}
            for key in ('velocity_relative_error', 'pressure_relative_error'):
                ratio = result['controls'][str(n)][key] / result['controls'][str(n // 2)][key]
                require(ratio <= (.4 if n == 64 else .5), str(n) + ': accuracy refinement ' + key)
                orders[key] = math.log2(1 / ratio)
            result['observed_orders'][str(n)] = orders
        for n, targets in ((32, (.03, .01)), (64, (.01, .003))):
            for key, target in zip(('velocity_relative_error', 'pressure_relative_error'), targets):
                require(result['controls'][str(n)][key] <= target, str(n) + ': accuracy target ' + key)
        result['processes']['integrals'] = execute(
            [str(directory / 'integrals')], directory, 'integrals', 30, 1536 * 1024**2)
        spec = importlib.util.spec_from_file_location('frozen_independent_forcing',
                frozen / 'scripts/verify_cfd_native_manufactured_stokes.py')
        verifier = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verifier)
        x, weights = np.polynomial.legendre.leggauss(6)
        low, high = [.25, .25, .25], [1.25, 1.75, 1.75]
        samples = json.loads((directory / 'integrals.stdout').read_text())
        require(len(samples) == 84, 'Integral sample identity')
        maximum = 0.
        for axis, a, b, derivative, value in samples:
            require(axis in (0, 1, 2) and derivative in (0, 1, 2, 3) and b > a,
                    'Malformed forcing integral')
            left, right = max(a, low[axis]), min(b, high[axis])
            expected = 0.
            if right > left:
                points = (right + left) / 2 + (right - left) * x / 2
                t = (points - low[axis]) / (high[axis] - low[axis])
                expected = float(np.dot(weights, verifier.derivative(t, derivative)) /
                                 (high[axis] - low[axis])**derivative * (right - left) / (2 * (b - a)))
            require(math.isfinite(value), 'Nonfinite forcing integral')
            maximum = max(maximum, abs(value - expected))
        require(maximum < 2e-8, 'Independent physical forcing agreement')
        result['independent_integral_samples'] = len(samples)
        result['maximum_independent_integral_error'] = maximum
        for name, digest in sources.items():
            require(sha(frozen / name) == digest == sha(ROOT / name), 'Source drift: ' + name)
        result['status'] = 'passed_smooth_native_accuracy_regression'
    except Exception as error:
        result['failure'] = str(error)
    result['artifact_sha256'] = {str(path.relative_to(directory)): sha(path)
                                 for path in sorted(directory.rglob('*')) if path.is_file()}
    save(directory / 'receipt.json', portable_paths(result, directory))
    seal_bundle(directory)
    print(json.dumps(dict(status=result['status'], receipt=str(directory / 'receipt.json'),
                          sha256=sha(directory / 'receipt.json'))), flush=True)
    return 0 if result['status'] == 'passed_smooth_native_accuracy_regression' else 1


if __name__ == '__main__':
    raise SystemExit(main())
