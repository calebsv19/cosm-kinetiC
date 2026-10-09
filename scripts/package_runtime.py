"""Immutable packaged workers; mutable execution evidence stays outside the package."""
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path

MANIFEST = 'coupling_payload.json'

def verify(root):
    root = Path(root).resolve()
    manifest = json.loads((root / MANIFEST).read_text())
    if manifest.get('schema') != 'physics_sim_coupling_payload/v1':
        raise ValueError('Unsupported coupling package inventory')
    for name, expected in manifest['files'].items():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Package inventory path escapes root')
        path = root / relative
        if any(p.is_symlink() for p in (path, *path.parents) if p != root.parent):
            raise ValueError('Package payload symlink')
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Package payload mismatch: ' + name)
    return manifest

def external(root, value):
    path = Path(os.path.abspath(value))
    if path.is_relative_to(root) or root.is_relative_to(path):
        raise ValueError('Package runtime/output must be outside the package')
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ValueError('Package runtime/output symlink')
    return path

@contextmanager
def execution(root, worker):
    manifest = verify(root)
    name = str(worker.relative_to(root))
    if name not in manifest['workers'].values() or name not in manifest['files']:
        raise ValueError('Worker is not declared by this package')
    value = os.environ.get('PHYSICS_SIM_PACKAGE_RUNTIME_ROOT')
    if not value or not Path(value).is_absolute():
        raise ValueError('Set an absolute PHYSICS_SIM_PACKAGE_RUNTIME_ROOT outside the package')
    runtime = external(root, value)
    runtime.mkdir(parents=True, exist_ok=True)
    fd = os.open(runtime / 'package-execution.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
        yield (fd,)
        if verify(root) != manifest:
            raise ValueError('Package inventory changed during execution')
    finally:
        os.close(fd)
