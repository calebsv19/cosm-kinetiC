"""Read-only exact evidence/restore coverage plan; never authorizes or performs pruning."""
import argparse
import json
import os
from pathlib import Path
import re
import stat
import time

from cfd_evidence import admitted_path, file_fingerprint, file_identity, verify_bundle
from check_clean_root import read_json
from retention_audit import policy
from restore_rehearsal import member_path


def snapshot(root, max_entries=100000, wall_cap=120):
    root = admitted_path(root)
    started = time.monotonic()
    files, directories = {}, {}
    pending = [root]
    entries = total = 0
    while pending:
        current = pending.pop()
        admitted_path(current)
        info = current.lstat()
        entries += 1
        if entries > max_entries or time.monotonic() - started > wall_cap:
            raise ValueError('Snapshot entry/time bound reached')
        name = str(current.relative_to(root))
        if stat.S_ISDIR(info.st_mode):
            directories[name] = file_identity(info)
            fd = os.open(current, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                if file_identity(os.fstat(fd)) != file_identity(info):
                    raise ValueError('Snapshot directory changed')
                with os.scandir(fd) as children:
                    for child in children:
                        if entries + len(pending) >= max_entries or time.monotonic() - started > wall_cap:
                            raise ValueError('Snapshot enumeration bound reached')
                        pending.append(current / child.name)
            finally:
                os.close(fd)
        elif stat.S_ISREG(info.st_mode) and info.st_nlink == 1:
            total += info.st_size
            if info.st_size > 8 * 1024**3 or total > 32 * 1024**3:
                raise ValueError('Snapshot byte bound reached')
            files[name] = file_fingerprint(current)
        else:
            raise ValueError('Snapshot holds links or special files: ' + str(current))
    if any(file_identity((root / name).stat()) != identity for name, identity in directories.items()):
        raise ValueError('Snapshot directory changed during readback')
    return {'files': files, 'directories': directories}


def sealed_receipt(path):
    path = admitted_path(path)
    before = snapshot(path.parent)
    verification = verify_bundle(path.parent)
    if path.name != 'receipt.json' or path.name not in before['files']:
        raise ValueError('Expected sealed root receipt.json')
    row = read_json(path, 1024 * 1024)
    if not isinstance(row, dict):
        raise ValueError('Receipt must be an object')
    if snapshot(path.parent) != before:
        raise ValueError('Sealed receipt changed during observation')
    return row, before, verification


def plan(repo, bundle, copy_receipt, restore_receipt):
    repo, bundle = admitted_path(repo), admitted_path(bundle)
    namespace = repo / 'data/experiments'
    if not bundle.is_relative_to(namespace) or bundle == namespace:
        raise ValueError('Select one sealed bundle below data/experiments')
    copy_receipt, restore_receipt = admitted_path(copy_receipt), admitted_path(restore_receipt)
    for protected in (copy_receipt.parent, restore_receipt.parent):
        if bundle == protected or bundle.is_relative_to(protected) or protected.is_relative_to(bundle):
            raise ValueError('Selected retirement bundle overlaps its recovery receipts')
    rules, policy_digest = policy(repo)
    source = snapshot(bundle)
    source_verification = verify_bundle(bundle)
    copied, copy_snapshot, copy_verification = sealed_receipt(copy_receipt)
    restored, restore_snapshot, restore_verification = sealed_receipt(restore_receipt)
    if copied.get('status') != 'verified_independent_cold_archive_copy':
        raise ValueError('Expected completed independent copy receipt, not preparation')
    if restored.get('schema') != 'physics_sim_restore_rehearsal_v1' or restored.get('status') != 'verified_independent_archive_retrieval_and_restore':
        raise ValueError('Expected completed independent retrieval/restore receipt')
    archive = copied.get('archive_destination')
    checksums = copied.get('payload_checksums')
    if not isinstance(archive, str) or not archive or not isinstance(checksums, dict) or not checksums:
        raise ValueError('Missing archive identity/checksums')
    if any(not isinstance(name, str) or not name or not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{64}', digest) for name, digest in checksums.items()):
        raise ValueError('Invalid archive checksum inventory')
    if (restored.get('source_copy_receipt_sha256') != copy_snapshot['files']['receipt.json']['sha256'] or
        restored.get('archive_destination') != archive or restored.get('payload_checksums') != checksums):
        raise ValueError('Restore receipt is not bound to selected archive copy')
    rows = restored.get('fresh_bundles_verified')
    if not isinstance(rows, list) or not rows:
        raise ValueError('Missing restored bundle inventory')
    names, matching = set(), None
    relative = str(bundle.relative_to(repo))
    for row in rows:
        if not isinstance(row, dict) or not isinstance(row.get('bundle'), str):
            raise ValueError('Malformed restored bundle row')
        name = row['bundle']
        if str(member_path(name)) != name or name in names:
            raise ValueError('Ambiguous restored bundle identity')
        names.add(name)
        if name == relative:
            matching = row
    if matching is None or matching.get('status') != 'verified' or type(matching.get('files')) is not int or matching['files'] != source_verification['files'] or matching.get('manifest_sha256') != source_verification['manifest_sha256']:
        raise ValueError('Current sealed bundle is outside verified snapshot coverage')
    restore_root = restored.get('restore_root')
    if not isinstance(restore_root, str) or not Path(restore_root).is_absolute():
        raise ValueError('Invalid restore root')
    readback = admitted_path(Path(restore_root) / 'fresh' / relative)
    if readback == bundle or readback.is_relative_to(bundle) or bundle.is_relative_to(readback):
        raise ValueError('Restore readback overlaps current bundle')
    recovery = snapshot(readback)
    recovery_verification = verify_bundle(readback)
    def bytes_only(observation):
        return {name: {key: row[key] for key in ('bytes', 'sha256')} for name, row in observation['files'].items()}
    if (recovery_verification != source_verification or bytes_only(source) != bytes_only(recovery) or
        set(source['directories']) != set(recovery['directories'])):
        raise ValueError('Current and restored full file inventories differ')
    for root, observed in ((bundle, source), (readback, recovery), (copy_receipt.parent, copy_snapshot), (restore_receipt.parent, restore_snapshot)):
        if snapshot(root) != observed:
            raise ValueError('Coverage inputs changed during planning')
    _, final_policy = policy(repo)
    if final_policy != policy_digest:
        raise ValueError('Retention policy changed during planning')
    return {'schema': 'physics_sim_evidence_retirement_plan_v1', 'status': 'covered_snapshot_held',
        'bundle': str(bundle), 'bundle_manifest': source_verification,
        'policy_version': rules['version'], 'policy_sha256': policy_digest,
        'copy_receipt': {'path': str(copy_receipt), 'sha256': copy_snapshot['files']['receipt.json']['sha256'], 'sealed_bundle': copy_verification},
        'restore_receipt': {'path': str(restore_receipt), 'sha256': restore_snapshot['files']['receipt.json']['sha256'], 'sealed_bundle': restore_verification},
        'archive_destination': archive, 'payload_checksums': checksums, 'restored_bundle': str(readback),
        'current_snapshot': source, 'restored_snapshot': recovery,
        'exact_file_coverage_verified': True, 'independent_retrieval_receipt_bound': True,
        'remote_archive_rechecked_now': False, 'terminal_ownership_verified': False,
        'pruning_authorized': False, 'eligible_for_pruning': False, 'mutations_performed': [],
        'holds': ['Class owner must establish terminal lifetime and retirement eligibility',
                  'Explicit guarded pruning and revalidation are not implemented by this read-only planner']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    for name in ('bundle', 'copy-receipt', 'restore-receipt'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(plan(args.repo, args.bundle, args.copy_receipt, args.restore_receipt), indent=2))
    except (OSError, ValueError, RecursionError, KeyError, TypeError) as error:
        parser.exit(2, 'Evidence retirement held; no mutation: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
