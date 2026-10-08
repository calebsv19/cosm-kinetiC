"""Read-only local build, cleanup and retention status; no activation or mutation."""
import argparse
import fcntl
import os
import json
from pathlib import Path
import subprocess
import stat
import re

from clean_outputs import ROOT_EXECUTABLES, plan, no_symlinks
from check_clean_root import unique_object
from build_owner import lock_path, ownership_paths
from cfd_evidence import sha, verify_bundle


def probe_lock(path, repo, mode=fcntl.LOCK_EX):
    """Observe kernel ownership without creating or modifying a lock file."""
    try:
        no_symlinks(path,repo)
        fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
    except FileNotFoundError:
        return {'state':'absent'}
    except (OSError,ValueError) as error:
        return {'state':'unverified','reason':str(error)}
    try:
        try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
        except BlockingIOError:return {'state':'held'}
        return {'state':'available'}
    finally:os.close(fd)


def read_local_bytes(path, repo, limit=1024*1024):
    """Bound local metadata reads and refuse links and non-regular inputs."""
    no_symlinks(path, repo)
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode):
            raise ValueError('Metadata must be a regular file')
        if info.st_size > limit:
            raise ValueError('Metadata exceeds byte limit: '+str(limit))
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            content = stream.read(limit+1)
        if len(content) > limit:
            raise ValueError('Metadata exceeds byte limit: '+str(limit))
        return content
    finally:
        os.close(fd)


def read_metadata(path, repo):
    row = json.loads(read_local_bytes(path, repo).decode('utf-8'), parse_constant=invalid_constant,object_pairs_hook=unique_object)
    if not isinstance(row, dict):
        raise ValueError('Metadata must be a JSON object')
    return row


def invalid_constant(value):
    raise ValueError('Non-finite JSON value: '+value)


def status(repo, build, tests, experiments, tools, executables=None):
    repo = repo.resolve()
    roots = {name: (repo/value if not value.is_absolute() else value) for name, value in (
        ('build', build), ('test', tests), ('experiments', experiments), ('tools', tools))}
    result = {'schema': 'physics_sim_local_status_v1', 'repo': str(repo),
              'roots': {name: {'path': str(path), 'exists': path.exists(), 'symlink': path.is_symlink()}
                        for name, path in roots.items()},
              'source_versions': {},
              'installed_or_published_state_verified': False}
    result['diagnostics'] = []
    for name, file in (('program', 'VERSION'), ('worker', 'WORKER_VERSION')):
        result['source_versions'][name] = None
        try:
            result['source_versions'][name] = read_local_bytes(repo/file, repo, 4096).decode('utf-8').strip()
        except FileNotFoundError:
            pass
        except (OSError, ValueError, RecursionError) as error:
            result['diagnostics'].append({'area': 'source_version', 'path': str(repo/file), 'status': 'unverified', 'reason': str(error)})

    try:
        git = subprocess.run(['git', '-C', str(repo), 'status', '--porcelain'], capture_output=True, text=True, timeout=15)
        result['source_status'] = {'git_available': git.returncode == 0,
                                   'changed_entries': len(git.stdout.splitlines()) if git.returncode == 0 else None}
    except (OSError, subprocess.TimeoutExpired) as error:
        result['source_status'] = {'git_available': False, 'changed_entries': None, 'reason': str(error)}
    try:
        result['cleanup'] = {'status': 'plan_available', 'plan': plan(repo, roots['build'],
            [roots['test'], roots['experiments'], roots['tools']] + [repo/n for n in (
                'src', 'include', 'scripts', 'tests', 'docs', 'make', 'data', 'export', 'dist', '.git')],
            executables if executables is not None else [roots['build']/'bin'/name for name in sorted(ROOT_EXECUTABLES)])}
    except (ValueError, OSError) as error:
        result['cleanup'] = {'status': 'held', 'reason': str(error)}
    hierarchy = []
    try:
        paths = ownership_paths(repo, Path(os.path.abspath(roots['build'])))
        for index, path in enumerate(paths[1:]):
            mode = fcntl.LOCK_EX if index == len(paths)-2 else fcntl.LOCK_SH
            hierarchy.append({'lock': str(path), 'required_mode': 'exclusive' if mode == fcntl.LOCK_EX else 'shared',
                              **probe_lock(path, repo, mode)})
    except ValueError as error:
        hierarchy.append({'state': 'unverified', 'reason': str(error)})
    result['ownership']={'selected_root':probe_lock(lock_path(repo,roots['build']),repo),
                         'hierarchy_admission':hierarchy,
                         'cleanup_exclusion':probe_lock(repo/'tmp/locks/clean.lock',repo),
                         'scope':'cooperative top-level Make ownership; direct external compiler writes are unverified'}
    active = roots['build']/'.configuration/active.json'
    result['build_configuration'] = None
    try:
        config = read_metadata(active, repo)
        if not isinstance(config.get('digest'), str) or not re.fullmatch('[0-9a-f]{64}', config['digest']):
            raise ValueError('Build configuration digest must be SHA-256')
        if type(config.get('generation')) is not int or config['generation'] < 1:
            raise ValueError('Build configuration generation must be a positive integer')
        result['build_configuration'] = config
    except FileNotFoundError:
        pass
    except (OSError, ValueError, RecursionError) as error:
        result['diagnostics'].append({'area': 'build_configuration', 'path': str(active), 'status': 'unverified', 'reason': str(error)})
    result['executables'] = {}
    for name in sorted(ROOT_EXECUTABLES):
        path = roots['build']/'bin'/name
        digest = None
        try:
            no_symlinks(path, repo)
            if path.is_file():
                digest = sha(path)
        except (OSError, ValueError, RecursionError) as error:
            result['diagnostics'].append({'area': 'executable', 'path': str(path), 'status': 'unverified', 'reason': str(error)})
        result['executables'][name] = {'path': str(path), 'exists': path.exists(), 'symlink': path.is_symlink(),
            'sha256': digest, 'profile_identity_verified': False}
    # Explicit receipt coverage is preserved; a prior backup is never generalized to later runs.
    result['backup_receipts'] = []
    result['archive_restore_rehearsals'] = []
    validation = roots['experiments']/'lifecycle-validation'
    if validation.is_dir() and not validation.is_symlink():
        for receipt in sorted(validation.glob('*/receipt.json')):
            try:
                row = read_metadata(receipt, repo)
                if row.get('status') == 'verified_independent_archive_retrieval_and_restore':
                    if (not isinstance(row.get('archive_destination'),str)
                            or not isinstance(row.get('source_copy_receipt_sha256'),str)
                            or not re.fullmatch('[0-9a-f]{64}',row['source_copy_receipt_sha256'])
                            or not isinstance(row.get('payload_checksums'),dict) or not row['payload_checksums']
                            or any(not isinstance(value,str) or not re.fullmatch('[0-9a-f]{64}',value) for value in row['payload_checksums'].values())
                            or row.get('historical_manifest_matched') is not True):
                        raise ValueError('Invalid archive retrieval rehearsal binding')
                    verified_record=verify_bundle(receipt.parent)
                    result['archive_restore_rehearsals'].append({'receipt':str(receipt),
                        'archive_destination':row['archive_destination'],
                        'source_copy_receipt_sha256':row['source_copy_receipt_sha256'],
                        'payload_checksums':row['payload_checksums'],'manifest_verification':verified_record,
                        'runtime_qualification_verified':False,'later_evidence_covered':False})
                    continue
                if row.get('status') != 'verified_independent_cold_archive_copy':
                    continue
                for field in ('archive_destination', 'coverage'):
                    if not isinstance(row.get(field), str) or not row[field].strip():
                        raise ValueError('Backup receipt requires non-empty '+field)
                rehearsal = row.get('remote_unpack_or_retrieval_rehearsal_performed', False)
                if type(rehearsal) is not bool:
                    raise ValueError('Backup restore rehearsal state must be boolean')
                try:
                    manifest_state = verify_bundle(receipt.parent)
                except (ValueError, OSError, KeyError, TypeError, AttributeError, RecursionError) as error:
                    manifest_state = {'status': 'unverified', 'reason': str(error)}
                result['backup_receipts'].append({'manifest_verification': manifest_state, 'receipt': str(receipt), 'receipt_sha256': sha(receipt),
                    'archive_destination': row['archive_destination'], 'coverage': row['coverage'],
                    'remote_unpack_or_retrieval_rehearsal_performed': rehearsal,
                    'payload_checksums':row.get('payload_checksums')})
            except (OSError, ValueError, RecursionError) as error:
                result['diagnostics'].append({'area': 'backup_receipt', 'path': str(receipt), 'status': 'unverified', 'reason': str(error)})
    for copied in result['backup_receipts']:
        copied['verified_retrieval_rehearsals']=[record for record in result['archive_restore_rehearsals']
            if record['source_copy_receipt_sha256']==copied['receipt_sha256']
            and record['archive_destination']==copied['archive_destination']
            and record['payload_checksums']==copied['payload_checksums']
            and copied['manifest_verification']['status']=='verified']
        copied['prepared_payload_retrieval_rehearsal_verified']=bool(copied['verified_retrieval_rehearsals'])
    result['backup_covers_all_current_evidence'] = False
    result['reference_python'] = {'path': str(roots['tools']/'cfd-reference-venv/bin/python'),
        'exists': (roots['tools']/'cfd-reference-venv/bin/python').is_file(), 'environment_qualification_verified': False}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    for name, default in (('build', 'build'), ('test', 'tmp/tests'), ('experiments', 'data/experiments'), ('tools', 'data/tools')):
        parser.add_argument('--'+name+'-root', type=Path, default=Path(default))
    parser.add_argument('--executable', type=Path, action='append')
    args = parser.parse_args()
    print(json.dumps(status(args.repo, args.build_root, args.test_root, args.experiments_root, args.tools_root, args.executable), indent=2))


if __name__ == '__main__': main()
