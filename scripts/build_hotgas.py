#!/usr/bin/env python3
"""Contained source-only hot-gas build. Output must be a fresh directory."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ['src/app/cfd_hotgas_caloric.c', 'src/app/cfd_hotgas3d.c', 'src/app/cfd_cartesian3d.c',
           'src/app/cfd_sparse_mg.c', 'src/app/cfd_memory.c']
HEADERS = ['include/app/cfd_hotgas_caloric.h', 'include/app/cfd_hotgas3d.h', 'include/app/cfd_cartesian3d.h',
           'include/app/cfd_sparse_mg.h', 'include/app/cfd_memory.h']


def build(output, sanitized=False, harness=False, domain=False, caloric_harness=False):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    entry = ('tests/cfd_hotgas_domain3d_test.c' if harness else 'src/tools/physics_sim_hotgas_domain_worker.c') if domain else ('tests/cfd_hotgas3d_test.c' if harness else 'src/tools/physics_sim_hotgas_worker.c')
    if caloric_harness:entry="tests/cfd_hotgas_caloric_test.c"
    sources = SOURCES + (['src/app/cfd_hotgas_domain3d.c', 'src/app/cfd_hotgas_sampling3d.c', 'src/app/cfd_hotgas_flux3d.c', 'src/app/cfd_hotgas_reconstruction3d.c', 'src/app/cfd_hotgas_rk2_3d.c', 'src/app/cfd_hotgas_lowmach_flux3d.c', 'src/app/cfd_hotgas_rk3_3d.c', 'src/app/cfd_hotgas_compression3d.c'] if domain else [])
    headers = HEADERS + (['include/app/cfd_hotgas_domain3d.h', 'include/app/cfd_hotgas_flux3d.h', 'include/app/cfd_hotgas_reconstruction3d.h'] if domain else [])
    files = sources + [entry] + headers
    pins = {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files}
    binary = output / ('native_test' if harness else ('physics_sim_hotgas_domain_worker' if domain else 'physics_sim_hotgas_worker'))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fno-fast-math', '-ffp-contract=off']
    flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitized else ['-O2']
    command = ['clang', *flags, '-Iinclude', *sources, entry, '-lm', '-o', str(binary)]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    (output/'compiler.log').write_text(result.stdout+result.stderr)
    if pins != {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files}:
        raise RuntimeError('source drift during build')
    receipt = dict(command=command, source_sha256=pins, returncode=result.returncode,
                   compiler=subprocess.check_output(['clang', '--version'], text=True))
    if result.returncode == 0:
        receipt['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    (output/'build.json').write_text(json.dumps(receipt, indent=2)+'\n')
    if result.returncode:
        raise RuntimeError(result.stderr)
    return binary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitized', action='store_true')
    parser.add_argument('--harness', action='store_true')
    parser.add_argument('--domain', action='store_true')
    parser.add_argument('--caloric-harness', action='store_true')
    args = parser.parse_args()
    print(build(args.output, args.sanitized, args.harness or args.caloric_harness, args.domain or args.caloric_harness, args.caloric_harness))
