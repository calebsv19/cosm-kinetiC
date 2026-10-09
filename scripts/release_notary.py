"""Retain exact notarization submissions and reconcile known IDs without resubmission.

Execution requires the owning release-control authority; this helper grants none.
"""
import argparse
import fcntl
import json
import os
import re
from pathlib import Path
import shutil
import sys
import uuid

from check_clean_root import read_json
from package_paths import no_symlinks
from contract_proof import execute, IncompleteTeardown
from desktop_replace import inventory, write_record
from package_outputs import plan
from package_transaction import input_identity, output_inventory, completed_evidence
from release_zip_validation import verify as verify_zip


def receipt_digest(path):
    return inventory(path,max_file_bytes=16777216,max_bytes=16777216)['.']['sha256']


def values(record):
    result = {}
    for item in record['source_identity']['values']:
        key, separator, value = item.partition('=')
        if not separator or key in result:
            raise ValueError('Invalid or duplicate notarization identity value')
        result[key] = value
    return result


def completed(repo, receipt):
    no_symlinks(receipt, repo)
    row = read_json(receipt, 16777216)
    if (not isinstance(row, dict) or row.get('schema') != 'physics_sim_package_transaction_v1'
            or not completed_evidence(receipt, row, repo)):
        raise ValueError('Notarization requires completed package transaction evidence')
    root = Path(row['output_root']); no_symlinks(root, repo)
    if receipt.parent.parent != root / '.package-transactions':
        raise ValueError('Package evidence is outside its exact transaction root')
    source=row['source_identity']
    if input_identity([Path(name) for name in source['inputs']],source['values'])[0] != row['identity']:
        raise ValueError('Notarization predecessor input identity changed')
    if output_inventory(root, row['contract']['outputs']) != row['output_inventory']:
        raise ValueError('Notarization predecessor output changed')
    return row


def archive_binding(repo, archive, receipt):
    record = completed(repo, receipt); selected = values(record)
    if selected.get('phase') != 'notary-archive':
        raise ValueError('Notarization requires exact notary-archive preparation')
    source = record['source_identity']['inputs']
    signed = Path(selected['signed_app']); signing_receipt = Path(selected['sign_receipt'])
    if str(signed) not in source or str(signing_receipt) not in source:
        raise ValueError('Notary archive lacks declared signing inputs')
    if selected['sign_receipt_sha256'] != receipt_digest(signing_receipt):
        raise ValueError('Signing receipt changed after archive preparation')
    signing = completed(repo, signing_receipt); signing_values = values(signing)
    if signing_values.get('phase') != 'sign' or signing_values.get('signing_identity') in (None, '', '-'):
        raise ValueError('Notarization requires a Developer ID signing stage')
    if not any(item['name'] == 'APP_DEST' and Path(signing['output_root']) / item['relative'] == signed
               for item in signing['contract']['outputs']):
        raise ValueError('Signed app does not match signing transaction')
    if source[str(signed)] != signing['output_inventory'][str(signed.relative_to(signing['output_root']))]:
        raise ValueError('Signed app changed between signing and archive preparation')
    if source[str(signing_receipt)]['.']['sha256'] != receipt_digest(signing_receipt):
        raise ValueError('Archive signing evidence changed')
    if not any(item['kind'] == 'file' and Path(record['output_root']) / item['relative'] == archive
               for item in record['contract']['outputs']):
        raise ValueError('Notary archive is not a declared prepared artifact')
    zip_proof=verify_zip(archive,signed)
    if zip_proof['archive_sha256']!=record['output_inventory'][str(archive.relative_to(record['output_root']))]['.']['sha256']:
        raise ValueError('Notary ZIP identity changed')
    return {'ordinary_zip_payload_verified':True,'archive_receipt_sha256': receipt_digest(receipt),
            'sign_receipt_sha256': receipt_digest(signing_receipt), 'signed_app': str(signed),
            'archive_sha256': record['output_inventory'][str(archive.relative_to(record['output_root']))]['.']['sha256']}


def response(path):
    row = read_json(path, 1048576)
    if not isinstance(row, dict) or not isinstance(row.get('id'), str):
        raise ValueError('Notary response lacks submission ID')
    if str(uuid.UUID(row['id'])) != row['id'].lower():
        raise ValueError('Notary response submission ID is not canonical UUID')
    row['id']=str(uuid.UUID(row['id']))
    return row


def retained_response(root, relative, repo):
    if not isinstance(relative,str) or not re.fullmatch(r'queries/[0-9a-f]{32}/notary.stdout',relative):
        raise ValueError('Invalid retained notary response path')
    path=root/relative;no_symlinks(path,repo)
    return response(path)


def run(repo, archive, root, archive_receipt, profile, tool, *, reconcile=False, run_command=execute):
    repo = repo.resolve(); archive = Path(os.path.abspath(archive))
    root = Path(os.path.abspath(root)); archive_receipt = Path(os.path.abspath(archive_receipt))
    if not profile or '\x00' in profile:
        raise ValueError('Notarization requires a keychain profile name')
    if archive.is_relative_to(root) or archive_receipt.is_relative_to(root):
        raise ValueError('Notary journal overlaps its inputs')
    plan(repo, root, [root / 'queries'], [])
    binding = archive_binding(repo, archive, archive_receipt)
    executable = shutil.which(tool)
    if executable is None: raise ValueError('Notary tool is unavailable')
    executable = str(Path(executable).resolve())
    inputs = [archive, archive_receipt, Path(executable), Path(__file__).resolve().parent]
    identity, observed = input_identity(inputs, ['profile=' + profile, json.dumps(binding, sort_keys=True)])
    receipt = root / 'receipt.json'; no_symlinks(receipt, repo)
    if root.exists() and not receipt.exists() and any(root.iterdir()):
        raise ValueError('Unknown notary journal predecessor retained')
    locks = repo / 'tmp/locks'; no_symlinks(locks, repo); locks.mkdir(parents=True, exist_ok=True)
    descriptors = []; state = None
    try:
        for path, mode in ((locks / 'clean.lock', fcntl.LOCK_SH), (root / 'owner.lock', fcntl.LOCK_EX)):
            path.parent.mkdir(parents=True, exist_ok=True); no_symlinks(path, repo)
            fd = os.open(path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600); descriptors.append(fd)
            try: fcntl.flock(fd, mode | fcntl.LOCK_NB)
            except BlockingIOError: raise ValueError('Notary journal held by active owner or cleanup')
        if receipt.exists():
            state = read_json(receipt, 16777216)
            if (not isinstance(state, dict) or state.get('schema') != 'physics_sim_notary_journal_v1'
                    or state.get('input_identity') != identity or state.get('binding') != binding):
                raise ValueError('Notary journal identity changed; no submission performed')
            if state.get('terminal_processes_verified') is not True:
                raise ValueError('Notary command teardown unverified; reconciliation held')
            if state['state'] == 'accepted':
                last=state['queries'][-1]
                proof=retained_response(root,last['directory']+'/notary.stdout',repo)
                if last['operation']!='info' or proof['id']!=state['submission_id'] or proof.get('status')!='Accepted':
                    raise ValueError('Accepted notarization proof changed')
                return state
            if not reconcile:
                raise ValueError('Existing notarization attempt retained; use reconciliation, never resubmit')
            submission_id = state.get('submission_id')
            if not submission_id:
                # A complete submit response may survive interruption before its
                # ID was journaled. Only that exact retained response can bind it.
                submission_id = retained_response(root,state['submit_stdout'],repo)['id']
            operation = 'info'
        else:
            if any(path.name!='owner.lock' for path in root.iterdir()):
                raise ValueError('Unknown notary journal predecessor retained under ownership')
            if reconcile: raise ValueError('No retained notarization submission to reconcile')
            state = {'schema': 'physics_sim_notary_journal_v1', 'artifact_class': 'release_artifact',
                     'state': 'prepared', 'binding': binding, 'input_identity': identity,
                     'archive_path':str(archive),'archive_receipt_path':str(archive_receipt),
                     'input_inventory': observed, 'submission_id': None,
                     'terminal_processes_verified': True, 'release_authority_granted': False,
                     'public_or_registry_acceptance_verified': False, 'queries': []}
            write_record(receipt, state); operation = 'submit'; submission_id = None
        query = root / 'queries' / uuid.uuid4().hex; no_symlinks(query,repo); query.mkdir(parents=True)
        stdout = query / 'notary.stdout'
        state['queries'].append({'operation': operation, 'directory': str(query.relative_to(root))})
        state['state'] = 'submission_uncertain' if operation == 'submit' else 'querying'
        state['terminal_processes_verified'] = False
        if operation == 'submit': state['submit_stdout'] = str(stdout.relative_to(root))
        else: state['submission_id'] = submission_id
        write_record(receipt, state)  # Durable intent precedes every external effect.
        argument = str(archive) if operation == 'submit' else submission_id
        argv = [executable, 'notarytool', operation, argument,
                '--keychain-profile', profile, '--output-format', 'json']
        if operation == 'submit': argv.append('--no-wait')
        try:
            run_command(argv, repo, query, 'notary', tuple(descriptors), 900, 1048576)
            state['terminal_processes_verified'] = True
            result = response(stdout)
            if operation == 'info' and result['id'] != submission_id:
                raise ValueError('Notary status response belongs to another submission')
            state['submission_id'] = result['id']
            if archive_binding(repo, archive, archive_receipt) != binding or input_identity(inputs, ['profile=' + profile, json.dumps(binding, sort_keys=True)])[0] != identity:
                raise ValueError('Notary inputs changed during command')
            if operation == 'submit': state['state'] = 'submitted'
            else:
                status = result.get('status')
                if status not in ('Accepted', 'Invalid', 'Rejected', 'In Progress'):
                    raise ValueError('Unknown notarization status')
                state['state'] = {'Accepted': 'accepted', 'In Progress': 'submitted'}.get(status, 'rejected')
                state['notary_status'] = status
            write_record(receipt, state)
            return state
        except BaseException as error:
            state['terminal_processes_verified'] = not isinstance(error, IncompleteTeardown)
            state['state'] = 'submission_uncertain' if operation == 'submit' else 'query_failed'
            state['failure'] = str(error); write_record(receipt, state)
            raise
    finally:
        for fd in descriptors: os.close(fd)


def accepted_binding(repo, receipt, signed_app):
    """Read-only exact acceptance gate for the downstream stapling stage."""
    receipt=Path(os.path.abspath(receipt));signed_app=Path(os.path.abspath(signed_app))
    no_symlinks(receipt,repo)
    if receipt.name!='receipt.json':raise ValueError('Acceptance requires exact journal receipt')
    root=receipt.parent;plan(repo,root,[root/'queries'],[])
    state=read_json(receipt,16777216)
    if (not isinstance(state,dict) or state.get('schema')!='physics_sim_notary_journal_v1'
            or state.get('state')!='accepted' or state.get('terminal_processes_verified') is not True):
        raise ValueError('Stapling requires verified Accepted notarization')
    archive=Path(state['archive_path']);archive_receipt=Path(state['archive_receipt_path'])
    binding=archive_binding(repo,archive,archive_receipt)
    if binding!=state['binding'] or binding['signed_app']!=str(signed_app):
        raise ValueError('Accepted notarization does not bind this signed app')
    source=state['input_inventory']
    if input_identity([Path(name) for name in source['inputs']],source['values'])[0]!=state['input_identity']:
        raise ValueError('Accepted notarization input identity changed')
    last=state['queries'][-1];relative=last['directory']+'/notary.stdout'
    proof=retained_response(root,relative,repo)
    if last['operation']!='info' or proof['id']!=state['submission_id'] or proof.get('status')!='Accepted':
        raise ValueError('Accepted notarization proof changed')
    return {'receipt_sha256':receipt_digest(receipt),'binding':binding,'submission_id':proof['id'],
            'inputs':[str(receipt),str(root/relative),str(archive),str(archive_receipt)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--archive-receipt', required=True, type=Path)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--tool', default='xcrun')
    parser.add_argument('--reconcile', action='store_true')
    args = parser.parse_args()
    try:
        print(json.dumps(run(Path.cwd(), args.archive, args.root, args.archive_receipt,
                             args.profile, args.tool, reconcile=args.reconcile)))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, 'Notarization held: ' + str(error) + '\n')


if __name__ == '__main__': main()
