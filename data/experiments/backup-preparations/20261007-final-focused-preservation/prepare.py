import hashlib, json, os, pathlib, shutil, stat, subprocess, tarfile, datetime

BASE = pathlib.Path(__file__).parent
TREE = BASE / 'snapshot'
TREE.mkdir(exist_ok=False)
CANON = pathlib.Path('/Users/calebsv/Desktop/CodeWork/physics_sim')
MAIN = pathlib.Path('/Users/calebsv/Desktop/CodeWork/_worktrees/physics_sim_main_edit')
rows = {}

def digest(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def identity(s):
    return (s.st_dev, s.st_ino, s.st_mode, s.st_size, s.st_mtime_ns, s.st_ctime_ns)

def copy(src, name):
    before = src.lstat()
    if not stat.S_ISREG(before.st_mode):
        raise RuntimeError('non-regular input: ' + str(src))
    dst = TREE / name
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        raise RuntimeError('duplicate member: ' + name)
    shutil.copy2(src, dst)
    sha = digest(dst)
    if identity(src.lstat()) != identity(before) or digest(src) != sha:
        raise RuntimeError('input changed: ' + str(src))
    rows[name] = {'bytes': before.st_size, 'sha256': sha, 'mode': stat.S_IMODE(before.st_mode)}

def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root)

source_states = {}
for label, root in [('canonical', CANON), ('main-edit', MAIN)]:
    state = {'root': str(root), 'head': git(root, 'rev-parse', 'HEAD').decode().strip(),
             'status_porcelain': git(root, 'status', '--porcelain=v1', '-z').decode(),
             'missing_tracked_or_selected': [], 'excluded_prefixes': ['data/experiments/', 'data/tools/']}
    names = sorted(set(git(root, 'ls-files', '-z', '--cached', '--others', '--exclude-standard').decode().split('\0')) - {''})
    selected = [n for n in names if not any(n.startswith(p) for p in state['excluded_prefixes'])]
    for name in selected:
        src = root / name
        if not src.exists():
            state['missing_tracked_or_selected'].append(name)
            continue
        copy(src, 'source/' + label + '/' + name)
    if state['head'] != git(root, 'rev-parse', 'HEAD').decode().strip() or state['status_porcelain'] != git(root, 'status', '--porcelain=v1', '-z').decode():
        raise RuntimeError('source selection drift: ' + label)
    source_states[label] = state

for label, root in [('main-edit-lifecycle', MAIN / 'data/experiments/lifecycle-validation'),
                    ('canonical-focused-cleanup', CANON / 'data/experiments/focused-cleanup')]:
    for p in sorted(root.rglob('*')):
        if p.is_dir() and not p.is_symlink():
            continue
        copy(p, 'evidence/' + label + '/' + p.relative_to(root).as_posix())

copy(BASE / 'packet-verification.json', 'metadata/packet-verification.json')
copy(pathlib.Path('/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/physics_sim/doc_sync_updates.md'), 'metadata/private-doc-sync-updates.md')
old = pathlib.Path('/private/tmp/physics-lifecycle-preservation-20261007b')
for name in ['preparation.json', 'current-copy-readback.json']:
    copy(old / name, 'metadata/previous-local-preparation/' + name)

def add_text(name, value):
    p = BASE / ('generated-' + pathlib.Path(name).name)
    p.write_text(value)
    copy(p, name)

add_text('metadata/source-states.json', json.dumps(source_states, indent=2) + '\n')
add_text('RESTORE.md', '''# PhysicsSim focused preservation snapshot

Extract into an EMPTY directory, never over an active checkout. Accept only the
regular archive members listed in coverage.json, plus coverage.json itself.
Reject absolute paths, parent traversal, links, devices, duplicates and extras.
Verify each file's SHA-256, byte size and mode against coverage.json.

source/canonical and source/main-edit contain present tracked and nonignored
source files selected with git ls-files. They are independent working-tree
snapshots, not commits and not a runnable full CodeWork workspace. Consult
metadata/source-states.json for base commits, status, exact missing/deleted
paths and exclusions. Do not restore a missing tracked path from a base commit
without reviewing the recorded deletion. Shared sibling libraries, Git history,
ignored build/package/runtime outputs, reference environments and other
experiment families are outside this snapshot.

evidence/main-edit-lifecycle preserves every regular file from the selected
167 lifecycle packets. metadata/packet-verification.json records current CFD
verifier results: alternate/older manifests and unsealed packets are preserved
as-is, not promoted to verified CFD bundles. The outer SHA inventory protects
their bytes. evidence/canonical-focused-cleanup preserves all five adoption
receipts. Later changes after this frozen snapshot require another backup.

The earlier independently retrieved CFD archive remains separate and must be
retained at /mnt/cold_archive_500g/codework-archive/generated-runs/
physics-cfd-lifecycle-20261007a/20261007T051233Z--physics-cfd-lifecycle-20261007a.
The earlier 20261007b local staged bundle was not uploaded; this snapshot adds
later source and receipts without overwriting that local preparation.

A local restore proves archive integrity only. Independent backup is complete
only after the stored PC cold archive payload is retrieved and verified.
''')

# Recheck all source/evidence bytes at the frozen selection before sealing.
for name, row in rows.items():
    p = TREE / name
    if p.stat().st_size != row['bytes'] or digest(p) != row['sha256']:
        raise RuntimeError('snapshot changed: ' + name)
coverage = {'schema': 'physics_sim_focused_preservation_v1',
            'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'files': rows, 'file_count': len(rows), 'logical_bytes': sum(r['bytes'] for r in rows.values()),
            'independent_copy_verified': False}
(TREE / 'coverage.json').write_text(json.dumps(coverage, indent=2, sort_keys=True) + '\n')
queue = BASE / 'queue' / 'physics-focused-preservation-20261007d'
queue.mkdir(parents=True, exist_ok=False)
archive = queue / 'physics-focused-preservation.tar.gz'
with tarfile.open(archive, 'w:gz', compresslevel=6) as tf:
    for name in sorted([*rows, 'coverage.json']):
        tf.add(TREE / name, arcname=name, recursive=False)

restore = BASE / 'local-readback'
restore.mkdir(exist_ok=False)
with tarfile.open(archive, 'r:gz') as tf:
    members = tf.getmembers()
    names = [m.name for m in members]
    expected = set(rows) | {'coverage.json'}
    if len(names) != len(set(names)) or set(names) != expected:
        raise RuntimeError('archive inventory mismatch')
    for m in members:
        p = pathlib.PurePosixPath(m.name)
        if not m.isfile() or p.is_absolute() or '..' in p.parts:
            raise RuntimeError('unsafe archive member')
        dst = restore / m.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        with tf.extractfile(m) as src, dst.open('xb') as out:
            shutil.copyfileobj(src, out)
        os.chmod(dst, m.mode)
for name, row in rows.items():
    p = restore / name
    if p.stat().st_size != row['bytes'] or digest(p) != row['sha256'] or stat.S_IMODE(p.stat().st_mode) != row['mode']:
        raise RuntimeError('restore mismatch: ' + name)
if digest(restore / 'coverage.json') != digest(TREE / 'coverage.json'):
    raise RuntimeError('coverage changed')
result = {'archive': str(archive), 'bytes': archive.stat().st_size, 'sha256': digest(archive),
          'files': len(rows), 'logical_bytes': coverage['logical_bytes'], 'local_restore_verified': True,
          'independent_copy_verified': False, 'source_states': {k:{'head':v['head'], 'missing':v['missing_tracked_or_selected']} for k,v in source_states.items()}}
(BASE / 'preparation.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
