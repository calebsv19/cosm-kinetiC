"""Verify a retrieved frozen archive without extracting any member."""
import hashlib, json, pathlib, sys, tarfile

archive = pathlib.Path(sys.argv[1])
expected_sha = sys.argv[2]
with archive.open('rb') as stream:
    actual_sha = hashlib.file_digest(stream, 'sha256').hexdigest()
if actual_sha != expected_sha:
    raise SystemExit('Payload digest mismatch')
with tarfile.open(archive, 'r:gz') as tf:
    members = tf.getmembers()
    names = [m.name for m in members]
    if len(set(names)) != len(names):
        raise SystemExit('Duplicate member')
    for member in members:
        name = pathlib.PurePosixPath(member.name)
        if not member.isfile() or name.is_absolute() or '..' in name.parts:
            raise SystemExit('Unsafe member')
    manifest_member = tf.getmember('coverage.json')
    if manifest_member.size > 16 * 1024 * 1024:
        raise SystemExit('Oversized coverage manifest')
    coverage = json.load(tf.extractfile(manifest_member))
    expected = coverage['files']
    if coverage['schema'] != 'physics_sim_focused_preservation_v1' or set(names) != set(expected) | {'coverage.json'}:
        raise SystemExit('Inventory mismatch')
    total = 0
    for member in members:
        if member.name == 'coverage.json':
            continue
        row = expected[member.name]
        if member.size != row['bytes'] or member.mode != row['mode']:
            raise SystemExit('Size/mode mismatch: ' + member.name)
        with tf.extractfile(member) as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != row['sha256']:
                raise SystemExit('Content mismatch: ' + member.name)
        total += member.size
    if total != coverage['logical_bytes'] or len(expected) != coverage['file_count']:
        raise SystemExit('Coverage count mismatch')
print(json.dumps({'status': 'verified', 'archive': str(archive), 'sha256': actual_sha,
                  'files': len(expected), 'logical_bytes': total, 'bytes': archive.stat().st_size}))
