"""Portable, digest-bound retained experiments for trusted local source workflows.

A manifest detects corruption; it is not a backup or physical qualification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import re

from check_clean_root import read_json


def admitted_path(value):
    """Normalize known macOS system aliases, then refuse other symlink components."""
    path = Path(os.path.abspath(value))
    for prefix, destination in (('/tmp', '/private/tmp'), ('/var', '/private/var')):
        alias = Path(prefix)
        if path.is_relative_to(alias) and alias.is_symlink() and alias.resolve() == Path(destination):
            path = Path(destination)/path.relative_to(alias)
    current = path
    while current != current.parent:
        if current.is_symlink():
            raise ValueError('Retained path refuses symlink component: '+str(current))
        current = current.parent
    return path


def file_identity(row):
    return (row.st_dev, row.st_ino, row.st_mode, row.st_size, row.st_mtime_ns, row.st_ctime_ns)


def file_fingerprint(path):
    path = admitted_path(path)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode):
            raise ValueError('Retained artifact must be a regular file: '+str(path))
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        after = os.fstat(fd)
        if file_identity(before) != file_identity(after) or file_identity(after) != file_identity(path.stat()):
            raise ValueError('Retained artifact changed during digest: '+str(path))
        return {'sha256': digest, 'bytes': after.st_size, 'identity': file_identity(after)}
    finally:
        os.close(fd)


def file_digest(path):
    row = file_fingerprint(path)
    return {key: row[key] for key in ('sha256', 'bytes')}


def sha(path):
    return file_fingerprint(path)['sha256']


def experiment_root(repo, explicit=None):
    repo = admitted_path(repo)
    root = admitted_path(explicit or os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT', repo/'data/experiments'))
    forbidden = [repo/name for name in ('build', 'tmp', 'src', 'include', 'scripts', 'tests',
                 'docs', 'make', 'config', 'third_party', '.git', '.agents', '.codex',
                 'export', 'dist', 'visual_artifacts', 'data/tools')]
    forbidden += [admitted_path(os.environ.get(key, repo/default)) for key, default in (
        ('PHYSICS_SIM_BUILD_ROOT', 'build'), ('PHYSICS_SIM_TEST_ROOT', 'tmp/tests'),
        ('PHYSICS_SIM_REFERENCE_TOOLS_ROOT', 'data/tools'))]
    if root.exists() and not root.is_dir():
        raise ValueError('Retained experiment root must be a directory')
    for path in forbidden:
        path = admitted_path(path)
        if root == path or root.is_relative_to(path) or path.is_relative_to(root):
            raise ValueError('Retained experiments overlap protected storage: '+str(root))
    return root


def resolve_artifact(bundle, reference):
    """Admit only canonical bundle-relative regular files, including after relocation."""
    if not isinstance(reference, str) or not reference:
        raise ValueError('Artifact reference must be a nonempty relative path')
    relative = Path(reference)
    if relative.is_absolute() or '..' in relative.parts or str(relative) != reference or str(relative) == '.':
        raise ValueError('Artifact must be canonical bundle-relative: '+str(reference))
    root = admitted_path(bundle)
    target = admitted_path(root/relative)
    if not target.is_relative_to(root) or not target.is_file():
        raise ValueError('Missing or escaping artifact: '+str(reference))
    return target


def regular_inventory(directory):
    directory = admitted_path(directory)
    if not directory.is_dir():
        raise ValueError('Retained bundle must be a directory')
    files = {}
    for path in sorted(directory.rglob('*')):
        admitted_path(path)
        mode = path.lstat().st_mode
        if stat.S_ISDIR(mode):
            continue
        if not stat.S_ISREG(mode):
            raise ValueError('Unclassified non-regular retained artifact: '+str(path))
        files[str(path.relative_to(directory))] = path
    return directory, files


def portable_paths(value, directory):
    """Normalize internal paths in the summary, leaving external provenance alone."""
    root = str(directory.resolve())
    if isinstance(value, dict):
        return {key: portable_paths(item, directory) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [portable_paths(item, directory) for item in value]
    if isinstance(value, str):
        # Includes paths embedded in compiler arguments. These remain provenance,
        # not executable replay commands after relocation.
        return value.replace(root+'/', '')
    return value


def seal_bundle(directory):
    """Seal once after all children terminate; existing identities never overwrite."""
    directory, inventory = regular_inventory(directory)
    if 'bundle_manifest.json' in inventory:
        raise ValueError('Retained bundle is already sealed')
    files = {name: file_digest(path) for name, path in inventory.items() if path.name != 'service.lock'}
    if not files:
        raise ValueError('Cannot seal an empty retained bundle')
    manifest = {'schema': 'physics_sim_retained_bundle_v1', 'files': files,
                'backup_verified': False, 'physical_accuracy_certified': False}
    with (directory/'bundle_manifest.json').open('x') as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write('\n'); stream.flush(); os.fsync(stream.fileno())
    return verify_bundle(directory)


def verify_bundle(directory):
    directory, inventory = regular_inventory(directory)
    manifest_path = directory/'bundle_manifest.json'
    manifest_identity = file_identity(manifest_path.stat())
    manifest = read_json(manifest_path, 64*1024*1024)
    if (not isinstance(manifest, dict) or manifest.get('schema') != 'physics_sim_retained_bundle_v1'
            or not isinstance(manifest.get('files'), dict) or not manifest['files']):
        raise ValueError('Invalid or empty retained manifest')
    for field in ('backup_verified', 'physical_accuracy_certified'):
        if type(manifest.get(field)) is not bool:
            raise ValueError('Retained manifest requires boolean '+field)
    actual = {name for name, path in inventory.items() if path != manifest_path and path.name != 'service.lock'}
    if actual != set(manifest['files']):
        raise ValueError('Retained file inventory changed')
    inspected = {}
    for name, expected in manifest['files'].items():
        if (not isinstance(expected, dict) or type(expected.get('bytes')) is not int or expected['bytes'] < 0
                or not isinstance(expected.get('sha256'), str) or not re.fullmatch('[0-9a-f]{64}', expected['sha256'])):
            raise ValueError('Invalid retained artifact metadata: '+str(name))
        path = resolve_artifact(directory, name)
        observed = file_fingerprint(path)
        inspected[name] = observed['identity']
        if any(observed[key] != expected[key] for key in ('bytes', 'sha256')):
            raise ValueError('Retained artifact changed: '+name)
    _, final_inventory = regular_inventory(directory)
    if (set(final_inventory) != set(inventory) or file_identity(manifest_path.stat()) != manifest_identity
            or any(file_identity(final_inventory[name].stat()) != identity for name, identity in inspected.items())):
        raise ValueError('Retained inventory or manifest changed during readback')
    final_manifest = file_fingerprint(manifest_path)
    if final_manifest['identity'] != manifest_identity:
        raise ValueError('Retained manifest changed during digest')
    return {'status': 'verified', 'files': len(actual), 'manifest_sha256': final_manifest['sha256']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify_bundle(args.bundle)))
    except (ValueError, OSError, RecursionError) as error:
        parser.exit(2, 'Retained readback held: '+str(error)+'\n')


if __name__ == '__main__':
    main()
