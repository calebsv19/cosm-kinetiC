"""Plan and remove only declared local compilation outputs.

Retained runs, environments, packages and source are never implicit clean targets.
The complete plan is validated before any mutation; apply rechecks identities.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import stat

from check_clean_root import check, reservation_scan
from build_outputs import ROOT_EXECUTABLES, inventory, verified


def no_symlinks(path, repo):
    current = path
    while current != repo:
        if current.is_symlink():
            raise ValueError('Cleanup refuses symlink component: '+str(current))
        if current == current.parent:
            raise ValueError('Cleanup target escapes checkout')
        current = current.parent


def identity(path):
    if not path.exists():
        return None
    row = path.stat()
    return [row.st_dev, row.st_ino, row.st_mode, row.st_size, row.st_mtime_ns]


def plan(repo, build, protected, executables):
    original_repo = Path(os.path.abspath(repo))
    repo = repo.resolve()
    def local(value):
        path = Path(os.path.abspath(value))
        return repo/path.relative_to(original_repo) if path.is_relative_to(original_repo) else path
    build = local(build)
    no_symlinks(build, repo)
    reservations = check(repo, build, protected)
    # An arbitrary top-level folder is not a compilation namespace.
    if not build.is_relative_to(repo/'build'):
        raise ValueError('Compilation cleanup must be inside checkout/build')
    paths = [build]
    for value in executables:
        path = local(value)
        no_symlinks(path, repo)
        if path.is_relative_to(build):
            continue  # Already covered by the validated directory.
        if path.parent != repo or path.name not in ROOT_EXECUTABLES:
            raise ValueError('Undeclared cleanup executable: '+str(path))
        if path.exists() and not stat.S_ISREG(path.stat().st_mode):
            raise ValueError('Cleanup executable is not a regular file: '+str(path))
        paths.append(path)
    owned = inventory(repo, build, extra_outputs=tuple(paths[1:]))
    if reservation_scan(repo,build)['witnesses'] != reservations:
        raise ValueError('Package reservations changed during cleanup plan hashing')
    return {'schema': 'physics_sim_clean_plan_v2', 'owned_files': owned, 'repo': str(repo),
            'build_root': str(build), 'targets': [
                {'path': str(p), 'identity': identity(p)} for p in dict.fromkeys(paths)]}


def apply(planned, validate):
    # Revalidate the whole set, not one target after earlier targets were removed.
    if validate() != planned:
        raise ValueError('Cleanup plan changed before apply')
    removed = []
    for row in planned['targets']:
        p = Path(row['path'])
        if identity(p) != row['identity']:
            raise ValueError('Cleanup target identity changed: '+str(p))
        if row['identity'] is None:
            continue
        if p.is_dir():
            if not shutil.rmtree.avoids_symlink_attacks:
                raise ValueError('Platform lacks safe directory removal')
            shutil.rmtree(p)
        else:
            p.unlink()
        removed.append(str(p))
    return {'status': 'cleaned', 'removed': removed, 'plan': planned}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('build-root', 'experiment-root', 'tools-root', 'test-root'):
        parser.add_argument('--'+option, type=Path, required=True)
    parser.add_argument('--executable', action='append', default=[], type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    repo = Path.cwd().resolve()
    protected = [args.experiment_root, args.tools_root, args.test_root] + [repo/n for n in (
        'src', 'include', 'scripts', 'tests', 'docs', 'make', '.git', '.agents',
        '.codex', 'config', 'third_party', 'data', 'export', 'dist')]
    validate = lambda: plan(repo, args.build_root, protected, args.executable)
    if not args.apply:
        print(json.dumps(validate(), indent=2))
        return
    # Serialize cleanup; this lock lives outside the removed build directory.
    lock_root = repo/'tmp/locks'
    no_symlinks(lock_root, repo)
    lock_root.mkdir(parents=True, exist_ok=True)
    descriptor = os.open(lock_root/'clean.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, 'r+') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.exit(2, 'Cleanup held by active build or cleanup; no outputs removed\n')
        print(json.dumps(apply(validate(), validate), indent=2))


if __name__ == '__main__':
    main()
