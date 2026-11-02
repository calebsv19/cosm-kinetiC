"""Build a source-bound Thermal worker for an existing joint coordinator.

Read-only numerical inputs; no imports, release builds or source adoption.
The producer identity stays pinned to the coordinator; receiver identity is
recorded separately because its numerical source may be a different version.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--coordinator-identity', type=Path, required=True)
p.add_argument('--thermal', type=Path, required=True)
p.add_argument('--shared', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
t, s, out = a.thermal.resolve(), a.shared.resolve(), a.output.resolve()
identity = json.loads(a.coordinator_identity.read_text())
assert identity['fixture']['profile'] in {'wall-refined-v1-n512-cn51200-ex6400', 'prescribed-shear-v1-nx128-nz256-ex5-6400'}
version = (s / 'core/core_thermal/VERSION').read_text().strip()
assert version in {'0.3.3', '0.3.4'}, 'unqualified receiver engine version'
assert not out.exists(), 'select fresh task-owned output'
assert not any(out == owner or out.is_relative_to(owner) for owner in (r, t, s)), 'keep output outside source owners'
worker = r / 'src/tools/physics_sim_receiver_worker.c'
fixture = r / 'src/tools/physics_sim_receiver_fixture.h'
wire = r / 'src/tools/physics_sim_receiver_wire.h'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
shear = r / 'src/tools/physics_sim_shear_receiver_fixture.h'
for path in (worker, fixture, wire, shear):
    assert identity['inputs'][str(path)] == sha(path), 'coordinator adapter source changed; rebuild coordinator'
core = [s / f'core/core_thermal/src/{name}.c' for name in
        ['core_thermal', 'core_thermal_rod', 'core_thermal_material', 'core_thermal_material_rod']]
core += [s / f'core/core_sim/src/{name}.c' for name in
         ['core_sim_time', 'core_sim_schedule', 'core_sim_exchange']]
host = [t / name for name in ['src/experiment/participant.c', 'src/coupling/inert.c', 'src/coupling/receiver.c']]
inputs = core + host + [worker, fixture, wire, shear, s / 'core/core_thermal/VERSION', Path(__file__).resolve()]
for directory in [s / 'core/core_thermal/include', s / 'core/core_sim/include',
                  t / 'include/thermal_sim', t / 'src/coupling']:
    inputs += list(directory.glob('*.h'))
before = {str(path): sha(path) for path in sorted(set(inputs))}
flags = [value for value in identity['flags'] if not value.startswith('-I')]
flags += ['-I' + str(path) for path in [r / 'include', r / 'src/tools', t / 'include',
          t / 'src/coupling', s / 'core/core_sim/include', s / 'core/core_thermal/include']]
strip = ['-Wl,-dead_strip'] if sys.platform == 'darwin' else ['-ffunction-sections', '-Wl,--gc-sections']
out.mkdir()
for name, extra in [('receiver-worker', []), ('receiver-worker-sanitize',
                     ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
    subprocess.run(flags + extra + strip + list(map(str, core + host + [worker])) +
                   ['-lm', '-o', str(out / name)], check=True)
assert all(sha(Path(path)) == digest for path, digest in before.items()), 'receiver source drift during build'
(out / 'identity.json').write_text(json.dumps(dict(
    engine=version, inputs=before, flags=flags, coordinator_source=identity['source'],
    receiver_source=hashlib.sha256(json.dumps(before, sort_keys=True).encode()).hexdigest(),
    binaries={name: sha(out / name) for name in ['receiver-worker', 'receiver-worker-sanitize']}
), indent=2) + '\n')
print('Source-bound strict/sanitizer Thermal receiver built; numerical owners unchanged.')
