#!/usr/bin/env python3
"""Retained per-user desktop installation with one atomic entry commit.

Standalone packaged stdlib helper; never launches an application or prunes data.
"""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys
import time
import uuid

LIMIT = 2097152
SCHEMA = 'physics_sim_desktop_entry_install_v1'


def path(value):
    selected = Path(value)
    if not selected.is_absolute() or '..' in selected.parts or len(str(selected)) > 4096:
        raise ValueError('Installer requires a bounded absolute path')
    if any(ord(c) < 32 or ord(c) == 127 for c in str(selected)):
        raise ValueError('Installer path contains control characters')
    # Standard macOS aliases are allowed for portable source fixtures only.
    for alias, target in (('/tmp', '/private/tmp'), ('/var', '/private/var')):
        prefix = Path(alias)
        if selected.is_relative_to(prefix) and prefix.is_symlink() and prefix.resolve() == Path(target):
            selected = Path(target) / selected.relative_to(prefix)
    for part in (selected, *selected.parents):
        if part.is_symlink():
            raise ValueError('Installer refuses linked path: ' + str(part))
    return selected


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(filename, limit=LIMIT):
    filename = path(filename)
    fd = os.open(filename, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit:
            raise ValueError('Installer file type/size held: ' + str(filename))
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            data = stream.read(limit + 1)
        after = os.fstat(fd)
        witness = lambda row: (row.st_dev, row.st_ino, row.st_mode, row.st_size, row.st_mtime_ns, row.st_ctime_ns)
        if len(data) > limit or witness(before) != witness(after) or witness(after) != witness(filename.lstat()):
            raise ValueError('Installer file changed during read')
        return data, stat.S_IMODE(after.st_mode)
    finally:
        os.close(fd)


def identity(filename, limit=LIMIT):
    filename = path(filename)
    if not filename.exists():
        return None
    data, mode = read(filename, limit)
    return {'sha256': sha(data), 'bytes': len(data), 'mode': mode}


def flush(directory):
    fd = os.open(path(directory), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def retain(filename, data, mode=0o600):
    filename = path(filename)
    fd = os.open(filename, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, mode)
    try:
        with os.fdopen(fd, 'wb', closefd=False) as stream:
            stream.write(data)
            stream.flush()
            os.fchmod(fd, mode)
            os.fsync(fd)
    finally:
        os.close(fd)
    flush(filename.parent)


def record(filename, row):
    data = (json.dumps(row, sort_keys=True, allow_nan=False) + '\n').encode()
    if len(data) > 65536:
        raise ValueError('Installer record byte limit')
    retain(filename, data)


def unique_object(pairs):
    row = {}
    for key, value in pairs:
        if key in row:
            raise ValueError('Duplicate installer record key')
        row[key] = value
    return row


def load(filename):
    data, _ = read(filename, 65536)
    row = json.loads(data, object_pairs_hook=unique_object,
                     parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite installer JSON')))
    if not isinstance(row, dict):
        raise ValueError('Installer record must be an object')
    return row


def escape(value):
    return value.replace('\\', '\\\\')


def desktop(launcher, icon):
    executable = str(launcher)
    if '=' in executable or not executable.isascii():
        raise ValueError('Desktop Exec path requires ASCII without equals sign')
    # Exec quoting is decoded after the ordinary desktop string escape layer.
    quoted = ''.join('\\' + c if c in '\\"`$' else c for c in executable)
    quoted = quoted.replace('%', '%%')
    return ('[Desktop Entry]\nType=Application\nName=kinetiC\n'
            'Comment=Physics simulation workspace\nExec="' + escape(quoted) + '"\n'
            'Icon=' + escape(str(icon)) + '\nTerminal=false\n'
            'Categories=Education;Science;Physics;\nStartupNotify=true\n').encode()


def locations(package, data_home):
    package = path(package)
    root = path(data_home)
    if root == Path('/') or root == package or root.is_relative_to(package):
        raise ValueError('Installer data root overlaps package or filesystem root')
    launcher = path(package / 'bin/physics-sim-launcher')
    icon_source = path(package / 'share/icons/hicolor/scalable/apps/kinetic.svg')
    if not read(launcher, 4194304)[1] & 0o111:
        raise ValueError('Package launcher is not executable')
    icon, _ = read(icon_source)
    if not icon:
        raise ValueError('Empty package icon')
    icons = path(root / 'icons/hicolor/scalable/apps')
    final_icon = path(icons / ('kinetic-' + sha(icon) + '.svg'))
    entry = path(root / 'applications/kinetic.desktop')
    store = path(root / 'PhysicsSim/desktop-entry-installs')
    rendered = desktop(launcher, final_icon)
    if len(rendered) > 65536:
        raise ValueError('Generated desktop entry byte limit')
    # Read every selected existing file before creating destinations or attempts.
    previous = identity(entry)
    legacy_icon = identity(icons / 'kinetic.svg')
    existing_icon = identity(final_icon)
    if existing_icon is not None and existing_icon != {'sha256': sha(icon), 'bytes': len(icon), 'mode': 0o644}:
        raise ValueError('Existing icon generation differs; retained without replacement')
    for directory in (root, entry.parent, icons, store):
        if directory.exists() and not directory.is_dir():
            raise ValueError('Installer destination ancestor is not a directory')
    read(path(package / 'share/install-desktop-entry.py'), 1048576)
    read(path(package / 'share/install-desktop-entry.sh'), 65536)
    return {'package': package, 'root': root, 'launcher': launcher, 'icon_source': icon_source,
            'icons': icons, 'icon': final_icon, 'entry': entry, 'store': store,
            'icon_data': icon, 'desktop_data': rendered, 'previous': previous, 'legacy_icon': legacy_icon,
            'launcher_identity': identity(launcher, 4194304), 'source_icon_identity': identity(icon_source),
            'installer_identity': identity(path(package / 'share/install-desktop-entry.py'), 1048576),
            'wrapper_identity': identity(path(package / 'share/install-desktop-entry.sh'), 65536)}


def pending(store):
    store = path(store)
    if not store.exists():
        return []
    result = []
    total = 0
    start = time.monotonic()
    with os.scandir(store) as entries:
        for index, item in enumerate(entries):
            if index >= 4096 or time.monotonic() - start > 10:
                raise ValueError('Installer history scan count/time bound')
            if item.name == 'owner.lock':
                if not stat.S_ISREG(item.stat(follow_symlinks=False).st_mode):
                    raise ValueError('Installer owner lock is not regular')
                continue
            attempt = path(store / item.name)
            if not re.fullmatch('[0-9a-f]{32}', item.name) or not attempt.is_dir():
                raise ValueError('Unknown installer history entry retained: ' + item.name)
            request = attempt / 'request.json'
            if not request.exists():
                result.append(item.name)
                continue
            row = load(request)
            if row.get('schema') != SCHEMA or row.get('attempt_id') != item.name:
                raise ValueError('Invalid installer history request')
            total += request.stat().st_size
            if total > 8388608:
                raise ValueError('Installer history aggregate byte bound')
            completed = attempt / 'completed.json'
            if completed.exists():
                done = load(completed)
                total += completed.stat().st_size
                if done != {'schema': SCHEMA, 'status': 'completed', 'request_sha256': sha(read(request, 65536)[0])}:
                    raise ValueError('Unbound installer completion record')
            else:
                result.append(item.name)
            if total > 8388608:
                raise ValueError('Installer history aggregate byte bound')
    return sorted(result)


@contextmanager
def owner(store):
    path(store).mkdir(parents=True, exist_ok=True)
    filename = path(store / 'owner.lock')
    fd = os.open(filename, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise ValueError('Installer owner lock is not a regular file')
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('Another desktop installer owns this data root')
        yield
    finally:
        os.close(fd)


def validate_request(loc, attempt):
    row = load(attempt / 'request.json')
    expected = {'schema', 'artifact_class', 'automatic_removal', 'attempt_id', 'package', 'data_home', 'entry', 'icon', 'launcher_identity',
                'source_icon_identity', 'installer_identity', 'wrapper_identity', 'previous', 'legacy_icon', 'desired_desktop_sha256', 'desired_icon_sha256'}
    if (set(row) != expected or row['schema'] != SCHEMA or row['attempt_id'] != attempt.name
            or row['artifact_class'] != 'retained_desktop_install_history' or row['automatic_removal'] is not False):
        raise ValueError('Invalid retained installer request schema')
    for key, value in (('package', str(loc['package'])), ('data_home', str(loc['root'])),
                       ('entry', str(loc['entry'])), ('icon', str(loc['icon'])),
                       ('launcher_identity', loc['launcher_identity']), ('source_icon_identity', loc['source_icon_identity']),
                       ('installer_identity', loc['installer_identity']), ('wrapper_identity', loc['wrapper_identity']),
                       ('desired_desktop_sha256', sha(loc['desktop_data'])), ('desired_icon_sha256', sha(loc['icon_data']))):
        if row[key] != value:
            raise ValueError('Installer recovery source/path mismatch: ' + key)
    for name, data in (('desired.desktop', loc['desktop_data']), ('desired.svg', loc['icon_data'])):
        if read(attempt / name)[0] != data:
            raise ValueError('Installer retained candidate changed')
    previous = row['previous']
    if previous is not None:
        old, mode = read(attempt / 'previous.desktop')
        if previous != {'sha256': sha(old), 'bytes': len(old), 'mode': mode}:
            raise ValueError('Installer predecessor snapshot mismatch')
    elif (attempt / 'previous.desktop').exists():
        raise ValueError('Unexpected installer predecessor snapshot')
    if row['legacy_icon'] is not None:
        old, mode = read(attempt / 'previous-legacy.svg')
        if row['legacy_icon'] != {'sha256': sha(old), 'bytes': len(old), 'mode': mode}:
            raise ValueError('Installer legacy icon snapshot mismatch')
    elif (attempt / 'previous-legacy.svg').exists():
        raise ValueError('Unexpected legacy icon snapshot')
    return row


def publish(loc, attempt, checkpoint=lambda state: None):
    row = validate_request(loc, attempt)
    current = identity(loc['entry'])
    desired = {'sha256': sha(loc['desktop_data']), 'bytes': len(loc['desktop_data']), 'mode': 0o644}
    if current != row['previous'] and current != desired:
        raise ValueError('Desktop entry changed since retained request; refusing replacement')
    checkpoint('before_icon')
    loc['icons'].mkdir(parents=True, exist_ok=True)
    existing = identity(loc['icon'])
    expected_icon = {'sha256': sha(loc['icon_data']), 'bytes': len(loc['icon_data']), 'mode': 0o644}
    if existing is None:
        candidate = path(loc['icons'] / ('.kinetic-' + uuid.uuid4().hex + '.pending.svg'))
        retain(candidate, loc['icon_data'], 0o644)
        os.link(candidate, path(loc['icon']), follow_symlinks=False)
        flush(loc['icons'])
    elif existing != expected_icon:
        raise ValueError('Icon generation changed; refusing replacement')
    checkpoint('after_icon')
    if identity(loc['icon']) != expected_icon:
        raise ValueError('Published icon readback mismatch')
    if identity(loc['entry']) == desired:
        return row
    if identity(loc['entry']) != row['previous']:
        raise ValueError('Desktop entry changed before publication')
    loc['entry'].parent.mkdir(parents=True, exist_ok=True)
    candidate = path(loc['entry'].parent / ('.kinetic-' + uuid.uuid4().hex + '.pending'))
    retain(candidate, loc['desktop_data'], 0o644)
    checkpoint('before_entry')
    # Re-admit sources and destination immediately before the commit point.
    validate_request(locations(loc['package'], loc['root']), attempt)
    if identity(loc['entry']) != row['previous']:
        raise ValueError('Desktop entry changed at publication')
    if row['previous'] is None:
        os.link(candidate, path(loc['entry']), follow_symlinks=False)
    else:
        os.replace(candidate, path(loc['entry']))
    flush(loc['entry'].parent)
    checkpoint('after_entry')
    if identity(loc['entry']) != desired or identity(loc['icon']) != expected_icon:
        raise ValueError('Desktop entry/icon publication readback mismatch')
    return row


def install(package, data_home, *, plan=False, recover=None, checkpoint=lambda state: None):
    loc = locations(package, data_home)
    if recover is not None and not re.fullmatch('[0-9a-f]{32}', recover):
        raise ValueError('Recovery requires an exact 32-hex attempt ID')
    if plan:
        return {'status': 'plan', 'entry': str(loc['entry']), 'icon': str(loc['icon']),
                'pending_attempts': pending(loc['store']), 'automatic_removal': False}
    with owner(loc['store']):
        loc = locations(package, data_home)
        unfinished = pending(loc['store'])
        if recover is None and unfinished:
            raise ValueError('Unfinished installer attempts retained; use --recover ID: ' + ','.join(unfinished[:8]))
        if recover is not None:
            attempt = path(loc['store'] / recover)
            if not attempt.is_dir():
                raise ValueError('Recovery attempt does not exist')
            if (attempt / 'completed.json').exists():
                validate_request(loc, attempt)
                if (identity(loc['entry']) != {'sha256': sha(loc['desktop_data']), 'bytes': len(loc['desktop_data']), 'mode': 0o644}
                        or read(loc['icon'])[0] != loc['icon_data']):
                    raise ValueError('Completed installation no longer matches current entry/icon')
                return {'status': 'already_completed', 'attempt': str(attempt), 'entry': str(loc['entry'])}
        else:
            attempt = loc['store'] / uuid.uuid4().hex
            attempt.mkdir(mode=0o700)
            for name, data in (('desired.desktop', loc['desktop_data']), ('desired.svg', loc['icon_data'])):
                retain(attempt / name, data, 0o644)
            if loc['previous'] is not None:
                data, mode = read(loc['entry'])
                retain(attempt / 'previous.desktop', data, mode)
            if loc['legacy_icon'] is not None:
                data, mode = read(loc['icons'] / 'kinetic.svg')
                retain(attempt / 'previous-legacy.svg', data, mode)
            record(attempt / 'request.json', {'schema': SCHEMA, 'artifact_class': 'retained_desktop_install_history',
                'automatic_removal': False, 'attempt_id': attempt.name,
                'package': str(loc['package']), 'data_home': str(loc['root']), 'entry': str(loc['entry']),
                'icon': str(loc['icon']), 'launcher_identity': loc['launcher_identity'],
                'source_icon_identity': loc['source_icon_identity'],
                'installer_identity': loc['installer_identity'], 'wrapper_identity': loc['wrapper_identity'],
                'previous': loc['previous'],
                'legacy_icon': loc['legacy_icon'], 'desired_desktop_sha256': sha(loc['desktop_data']),
                'desired_icon_sha256': sha(loc['icon_data'])})
            flush(loc['store'])
        try:
            checkpoint('prepared')
            publish(loc, attempt, checkpoint)
            record(attempt / 'completed.json', {'schema': SCHEMA, 'status': 'completed',
                   'request_sha256': sha(read(attempt / 'request.json', 65536)[0])})
            return {'status': 'completed', 'attempt': str(attempt), 'entry': str(loc['entry'])}
        except BaseException as error:
            record(attempt / ('failure-' + uuid.uuid4().hex + '.json'),
                   {'schema': SCHEMA, 'status': 'held', 'failure': str(error)[:4096]})
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    options = parser.add_mutually_exclusive_group()
    options.add_argument('--plan', action='store_true')
    options.add_argument('--recover')
    args = parser.parse_args()
    try:
        package = Path(os.path.abspath(__file__)).parent.parent
        data_home = os.environ.get('XDG_DATA_HOME') or str(Path.home() / '.local/share')
        result = install(package, data_home, plan=args.plan, recover=args.recover)
        print(json.dumps(result) if args.plan else result['entry'])
    except (OSError, ValueError, RecursionError) as error:
        parser.exit(2, str(error) + '\n')


if __name__ == '__main__':
    main()
