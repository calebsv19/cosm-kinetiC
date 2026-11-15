"""Use the existing owned Registry attempt slots without writable source inputs.

This is path admission only, never release authority or cleanup permission.
Without an explicit isolated build slot, ordinary checkout paths are unchanged.
"""
import json
import os
from pathlib import Path
import re


def physical(path):
    path = Path(path)
    if not path.is_absolute() or '..' in path.parts:
        raise ValueError('Output layout requires absolute paths without traversal')
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError('Output layout refuses symlink components')
    return path


def read_record(path):
    physical(path)
    with path.open('rb') as stream:
        raw = stream.read(16385)
    if len(raw) > 16384:
        raise ValueError('Output layout ownership record exceeds bound')
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError('Output layout ownership record must be an object')
    return value


def attempt(repo):
    selected = os.environ.get('PHYSICS_SIM_LAYOUT_BUILD')
    if not selected:
        return None
    build = physical(Path(selected))
    root = build.parent
    if build.name != 'build' or repo != root/'source/physics_sim':
        raise ValueError('Output layout does not select this frozen source')
    marker = read_record(root/'.task-output-owner.json')
    info = root.stat()
    if (marker.get('schema') != 'owned-task-output/v1' or marker.get('root') != str(root)
            or marker.get('device') != info.st_dev or marker.get('inode') != info.st_ino):
        raise ValueError('Output layout attempt ownership changed')
    descriptor = read_record(physical(Path(marker['owner'])))
    if (descriptor.get('attempt_root') != str(root)
            or not re.fullmatch('rapcj_[0-9a-f]{64}', descriptor.get('job_id', ''))
            or not re.fullmatch('rapcl_[0-9a-f]{64}', descriptor.get('lease_id', ''))
            or not re.fullmatch('[0-9a-f]{64}', descriptor.get('inputs_sha256', ''))):
        raise ValueError('Output layout attempt descriptor mismatch')
    return root


def build_root(repo):
    root = attempt(repo)
    return root/'build' if root else repo/'build'


def runtime_root(repo):
    root = attempt(repo)
    return root/'runtime' if root else repo/'tmp'


def test_root(repo):
    root = attempt(repo)
    return root/'tests' if root else repo/'tmp'


def no_symlinks(path, repo):
    """Admit source reads and exact isolated output slots; never their parent."""
    path = physical(path)
    root = attempt(repo)
    allowed = [repo]
    if root:
        allowed += [root/name for name in ('build', 'runtime', 'tests', 'package')]
    if not any(path == base or path.is_relative_to(base) for base in allowed):
        raise ValueError('Output path escapes checkout and owned attempt slots')


def output_key(path, repo):
    no_symlinks(path, repo)
    if path.is_relative_to(repo):
        return str(path.relative_to(repo))
    return '@attempt/' + str(path.relative_to(attempt(repo)))


def make_target(path, repo):
    no_symlinks(path, repo)
    return str(path.relative_to(repo)) if path.is_relative_to(repo) else str(path)


def package_root(path, repo):
    root = attempt(repo)
    if root:
        for base in (root/'package', root/'build/dist'):
            if path == base or path.is_relative_to(base):
                return base
    return None
