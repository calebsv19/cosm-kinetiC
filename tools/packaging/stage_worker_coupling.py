#!/usr/bin/env python3
"""Add the explicit offline adapter closure to a fresh worker staging directory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

MODULES = ('worker_package_entry', 'worker_package_cli', 'package_runtime', 'coupled_atmosphere', 'coupled_open_atmosphere',
    'coupled_passive', 'passive_atmosphere', 'evolving_atmosphere', 'open_atmosphere',
    'atmosphere_attempt', 'cfd_evidence', 'check_clean_root', 'clean_outputs', 'build_outputs',
    'build_owner', 'tool_probe', 'numeric_digest_stream')
WORKERS = {'session':'physics_sim_session_worker', 'passive':'physics_sim_passive_worker',
    'evolving':'physics_sim_atmosphere_worker', 'open':'physics_sim_open_atmosphere_worker'}

def stage(source, dest, workers):
    files = [source/'scripts'/ (name+'.py') for name in MODULES]
    files += sorted((source/'scripts/surface_sources').glob('*.py'))
    for original in files:
        target = dest / original.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    for role, name in WORKERS.items():
        shutil.copy2(workers[role], dest/'bin'/name)
    wrapper = dest/'bin/physics_sim_coupling'
    wrapper.write_text("""#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 -I -B "$HERE/../scripts/worker_package_entry.py" "$@"
""")
    wrapper.chmod(0o755)
    (dest/'examples').mkdir(exist_ok=True)
    shutil.copy2(source/'tests/fixtures/surface_sources/prescribed-policy.json', dest/'examples/prescribed-policy.json')
    payload = {str(p.relative_to(dest)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(dest.rglob('*')) if p.is_file()}
    manifest = {'schema':'physics_sim_coupling_payload/v1', 'files':payload,
        'workers':{role:'bin/'+name for role,name in WORKERS.items()},
        'entrypoint':'bin/physics_sim_coupling', 'coupling':'one_way_synthetic_source',
        'physical_calibration_qualified':False, 'bidirectional_feedback':False}
    (dest/'coupling_payload.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')

def main():
    p=argparse.ArgumentParser();p.add_argument('--stage',type=Path,required=True)
    for role in WORKERS:p.add_argument('--'+role,type=Path,required=True)
    a=p.parse_args();stage(Path(__file__).resolve().parents[2],a.stage, {r:getattr(a,r) for r in WORKERS})
if __name__=='__main__':main()
