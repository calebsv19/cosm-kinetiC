#!/usr/bin/env python3
"""Verify package bytes before importing any packaged adapter modules."""
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]


def main():
    inventory=ROOT/'coupling_payload.json'
    if inventory.is_symlink():raise ValueError('Package inventory symlink')
    manifest=json.loads(inventory.read_text())
    if manifest['schema']!='physics_sim_coupling_payload/v1':raise ValueError('Unsupported package inventory')
    actual={str(p.relative_to(ROOT)) for p in ROOT.rglob('*') if p.is_file() or p.is_symlink()}
    if actual != set(manifest['files']) | {'coupling_payload.json'}:raise ValueError('Package file inventory changed')
    for name,expected in manifest['files'].items():
        path=ROOT/name
        if path.is_symlink() or not path.resolve().is_relative_to(ROOT) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
            raise ValueError('Package payload mismatch: '+name)
    sys.path.insert(0,str(ROOT/'scripts'))
    from worker_package_cli import main as entry
    entry()

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError) as error:raise SystemExit(str(error))
