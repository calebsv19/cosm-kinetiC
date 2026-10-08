"""Own and retain macOS dependency bundling inside reserved local package output."""
import argparse
import fcntl
import hashlib
import math
import os
from pathlib import Path
import shutil
import sys
import uuid

from agent_session.owned_command import execute, IncompleteTeardown
from agent_session.sample_retention import regular, retain
from build_owner import inherited_descriptors
from cfd_evidence import admitted_path, experiment_root
from check_clean_root import read_json, reservation_output
from desktop_replace import inventory, InventoryBudget, write_record


def admission(repo, app_bin, frameworks):
    repo = admitted_path(repo)
    app_bin = admitted_path(app_bin)
    frameworks = admitted_path(frameworks)
    app = app_bin.parent.parent.parent
    if (not app.name.endswith('.app') or app_bin.parent != app / 'Contents/MacOS'
            or frameworks != app / 'Contents/Frameworks'):
        raise ValueError('Bundler requires one app Contents/MacOS binary and sibling Frameworks')
    if not (app.is_relative_to(repo / 'build') or app.is_relative_to(repo / 'dist')):
        raise ValueError('Bundler refuses outside local package namespaces')
    reservation = None
    for root in app.parents:
        if root == repo:
            break
        candidate = root / '.package-reservations' / (hashlib.sha256(str(app).encode()).hexdigest() + '.json')
        if candidate.exists() or candidate.is_symlink():
            candidate = admitted_path(candidate)
            row = read_json(candidate, 65536)
            if reservation_output(repo, candidate, row) != app:
                raise ValueError('Bundler reservation output mismatch')
            reservation = candidate
            break
    if reservation is None:
        raise ValueError('Bundler requires an exact package output reservation')
    if not app_bin.is_file() or not frameworks.is_dir():
        raise ValueError('Bundler requires assembled binary and frameworks directory')
    budget = InventoryBudget(max_entries=4096, max_bytes=536870912, max_file_bytes=134217728)
    observed = {'binary': inventory(app_bin, budget=budget), 'frameworks': inventory(frameworks, budget=budget)}
    if any(value.get('kind') == 'symlink' for rows in observed.values() for value in rows.values()):
        raise ValueError('Bundler refuses linked package payload')
    return app_bin, frameworks, reservation, observed


def run(repo, app_bin, frameworks, *, wall_cap=900, log_cap=67108864):
    if (type(wall_cap) not in (int, float) or not math.isfinite(wall_cap) or not 0 < wall_cap <= 3600
            or type(log_cap) is not int or not 1 <= log_cap <= 67108864):
        raise ValueError('Invalid bundler time/log bounds')
    repo = admitted_path(repo)
    app_bin, frameworks, reservation, before = admission(repo, app_bin, frameworks)
    parent = admitted_path(experiment_root(repo, repo / 'data/experiments') / 'package-bundler-attempts')
    for ancestor in (parent, *parent.parents):
        if ancestor == repo:
            break
        marker = ancestor / 'bundle_manifest.json'
        if marker.exists() or marker.is_symlink():
            raise ValueError('Bundler refuses sealed evidence namespaces')
    controls = [repo / 'scripts/macos_bundle.py', repo / 'scripts/macos_bundle_engine.sh',
                repo / 'tools/packaging/macos/bundle-dylibs.sh']
    captured = {p: regular(p, 4194304) for p in controls}
    reservation_bytes = regular(reservation, 65536)
    descriptors = []
    attempt = None
    state = None
    try:
        for path, mode in ((repo / 'tmp/locks/clean.lock', fcntl.LOCK_SH),
                (reservation.parent.parent / '.bundler-locks' / (reservation.stem + '.lock'), fcntl.LOCK_EX)):
            admitted_path(path)
            path.parent.mkdir(parents=True, exist_ok=True)
            fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
            descriptors.append(fd)
            try:
                fcntl.flock(fd, mode | fcntl.LOCK_NB)
            except BlockingIOError:
                raise ValueError('Bundling held by active cleanup or same-app bundler')
        if admission(repo, app_bin, frameworks)[3] != before or regular(reservation, 65536) != reservation_bytes:
            raise ValueError('Bundler input changed before ownership')
        parent.mkdir(parents=True, exist_ok=True)
        attempt = parent / ('bundle-' + uuid.uuid4().hex)
        attempt.mkdir(mode=0o700)
        for name in ('work', 'before', 'control'):
            (attempt / name).mkdir()
        fd = os.open(attempt / 'owner.lock', os.O_CREAT | os.O_EXCL | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        descriptors.append(fd)
        fcntl.flock(fd, fcntl.LOCK_EX)
        state = {'schema': 'physics_sim_macos_bundle_attempt_v1', 'artifact_class': 'retained_evidence',
                 'status': 'running', 'binary': str(app_bin), 'frameworks': str(frameworks),
                 'reservation': str(reservation), 'before_inventory': before,
                 'search_roots': os.environ.get('PACKAGE_DEP_SEARCH_ROOTS', '/opt/homebrew:/usr/local'),
                 'wall_cap_s': wall_cap, 'combined_log_cap_bytes': log_cap,
                 'automatic_removal': False, 'release_authority_granted': False,
                 'all_external_descendants_verified_terminal': False,
                 'terminal_verification_scope': 'Owned command/anchor and local operations only; complete descendants unverified',
                 'complete_dependency_capture': False, 'aggregate_output_storage_enforced': False,
                 'automatic_rollback_performed': False, 'commands': []}
        write_record(attempt / 'request.json', state)
        print('Bundler retained: ' + str(attempt), flush=True)
        retain(attempt / 'reservation.json', reservation_bytes)
        for path, data in captured.items():
            retain(attempt / 'control' / path.name, data)
        shutil.copy2(app_bin, attempt / 'before/binary')
        shutil.copytree(frameworks, attempt / 'before/Frameworks', symlinks=True)
        copied = {'binary': inventory(attempt / 'before/binary'),
                  'frameworks': inventory(attempt / 'before/Frameworks')}
        if copied != before or admission(repo, app_bin, frameworks)[3] != before:
            raise ValueError('Bundler predecessor snapshot mismatch')
        inherited = []
        owner = os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
        if owner:
            inherited = list(inherited_descriptors(repo, Path(owner)))
            if not inherited:
                raise ValueError('Bundler build ownership descriptors are invalid')
        state['commands'].append(execute(['/bin/sh', str(attempt / 'control/macos_bundle_engine.sh'),
            str(app_bin), str(frameworks), str(attempt / 'work')], repo, attempt, 'bundle',
            tuple(descriptors + inherited), wall_cap, log_cap))
        if regular(reservation, 65536) != reservation_bytes or any(regular(p, 4194304) != data for p, data in captured.items()):
            raise ValueError('Bundler control or reservation changed during execution')
        state['after_inventory'] = admission(repo, app_bin, frameworks)[3]
        state['status'] = 'passed'
        return attempt
    except BaseException as error:
        if state is not None:
            state['status'] = 'held' if isinstance(error, IncompleteTeardown) else 'failed'
            state['failure'] = str(error)[:4096]
        raise
    finally:
        try:
            if state is not None:
                write_record(attempt / 'receipt.json', state)
        finally:
            for fd in descriptors:
                os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app_bin', type=Path)
    parser.add_argument('frameworks', type=Path)
    args = parser.parse_args()
    try:
        run(Path(__file__).resolve().parents[1], args.app_bin, args.frameworks)
    except (ValueError, OSError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    main()
