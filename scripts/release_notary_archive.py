"""Prepare an exact signed-app notarization archive in a retained transaction."""
import argparse
import json
import os
from pathlib import Path
import sys

from contract_proof import execute
from desktop_replace import inventory
from package_outputs import plan, declare
from package_transaction import run, parse_assignments
from release_notary import completed, values, receipt_digest
from release_zip_validation import verify as verify_zip


def signing_input(repo, receipt):
    record = completed(repo, receipt); selected = values(record)
    if selected.get('phase') != 'sign' or selected.get('signing_identity') in (None, '', '-'):
        raise ValueError('Notary archive requires a Developer ID signing stage')
    apps = [Path(record['output_root']) / item['relative'] for item in record['contract']['outputs']
            if item['name'] == 'APP_DEST' and item['kind'] == 'directory']
    if len(apps) != 1: raise ValueError('Signing transaction requires one exact app')
    return apps[0], receipt_digest(receipt)


def assemble(repo, receipt, tool, assignments):
    if set(assignments) != {'RELEASE_ROOT', 'RELEASE_DIR', 'ARCHIVE', 'CHECKSUM', 'MANIFEST', 'REPORTS'}:
        raise ValueError('Notary archive requires exact transaction mappings')
    root = assignments['RELEASE_ROOT']
    if assignments['RELEASE_DIR'] != root: raise ValueError('Archive root mapping mismatch')
    archive = assignments['ARCHIVE']; checksum = assignments['CHECKSUM']
    manifest = assignments['MANIFEST']; reports = assignments['REPORTS']
    app, signed_hash = signing_input(repo, receipt)
    if app.is_relative_to(root) or root.is_relative_to(app) or receipt.is_relative_to(root):
        raise ValueError('Archive outputs overlap signing inputs')
    validation = lambda: plan(repo, root, [reports], [archive, checksum, manifest])
    declared = validation(); declare(declared, validation)
    execute([tool, '-c', '-k', '--sequesterRsrc', '--keepParent', str(app), str(archive)],
            repo, reports, 'archive', (), 900, 1048576)
    zip_proof=verify_zip(archive,app)
    row = inventory(archive)['.']
    if row['kind'] != 'file' or row['bytes'] == 0: raise ValueError('Empty or invalid notary archive')
    if signing_input(repo, receipt) != (app, signed_hash):
        raise ValueError('Signing input changed during archive preparation')
    with checksum.open('x') as stream:
        stream.write(row['sha256'] + '  ' + archive.name + '\n')
    with manifest.open('x') as stream:
        json.dump({'schema': 'physics_sim_notary_archive_v1', 'phase': 'notary-archive',
                   'artifact': archive.name, 'format': 'zip', 'sha256': row['sha256'],
                   'bytes': row['bytes'], 'signed_app': str(app), 'sign_receipt_sha256': signed_hash,
                   'release_authority_granted': False, 'public_acceptance_verified': False,
                   'zip_validation':zip_proof}, stream, indent=2)
        stream.write('\n')
    if inventory(archive)['.'] != row: raise ValueError('Archive changed during metadata creation')


def prepare(repo, receipt, root, tool='/usr/bin/ditto', archive_name='notary-upload.zip'):
    receipt = Path(os.path.abspath(receipt)); root = Path(os.path.abspath(root))
    if (not archive_name.endswith('.zip') or Path(archive_name).name != archive_name
            or any(character in archive_name for character in '\n\r\x00')):
        raise ValueError('Notary archive requires a simple ZIP filename')
    app, signed_hash = signing_input(repo, receipt)
    if app.is_relative_to(root) or root.is_relative_to(app) or receipt.is_relative_to(root):
        raise ValueError('Archive root overlaps signing inputs')
    helper = Path(__file__).resolve()
    return run(repo, root, {'REPORTS': root / 'command-reports'},
               {'ARCHIVE': root / archive_name, 'CHECKSUM': root / (archive_name + '.sha256'),
                'MANIFEST': root / (archive_name + '.manifest.json')}, {},
               [app, receipt, helper.parent],
               ['phase=notary-archive', 'signed_app=' + str(app), 'sign_receipt=' + str(receipt),
                'sign_receipt_sha256=' + signed_hash],
               [sys.executable, '-B', str(helper), '_assemble', '--sign-receipt', str(receipt), '--tool', tool], [tool])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', '_assemble'))
    parser.add_argument('--sign-receipt', required=True, type=Path)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--tool', default='/usr/bin/ditto')
    parser.add_argument('--archive-name', default='notary-upload.zip')
    args, mappings = parser.parse_known_args()
    try:
        repo = Path.cwd().resolve(); receipt = Path(os.path.abspath(args.sign_receipt))
        if args.action == '_assemble':
            if args.root: raise ValueError('Assembly root must come from transaction mappings')
            assemble(repo, receipt, args.tool, parse_assignments(mappings))
        else:
            if args.root is None or mappings: raise ValueError('Preparation requires root and no mappings')
            print(json.dumps(prepare(repo, receipt, args.root, args.tool, args.archive_name)))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, 'Notary archive held: ' + str(error) + '\n')


if __name__ == '__main__': main()
