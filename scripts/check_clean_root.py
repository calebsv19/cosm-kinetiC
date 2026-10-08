"""Fail closed before Make cleanup can touch retained or misconfigured roots."""
import argparse
import hashlib
import uuid
import json
import math
import stat
import os
import time
from pathlib import Path


HELD_DIRECTORIES = frozenset(('runs', 'observer-runs', 'source', 'cfd-reference-venv',
                              'release', 'release-authenticated', 'agent_runs', 'receipts'))


def unique_object(pairs):
    row={}
    for key,value in pairs:
        if key in row:raise ValueError('Duplicate JSON field: '+key)
        row[key]=value
    return row


def finite_constant(value):
    raise ValueError('Non-finite JSON value: '+value)


# Generic cleanup metadata may be an object, array or scalar. Unlike reference
# receipt admission, this helper does not impose a domain-specific root schema.
JSON_MAX_DEPTH = 64
JSON_MAX_EVENTS = 100000
JSON_MAX_NUMBER_CHARS = 128


def json_structure_admitted(text):
    """Count containers/separators outside strings before JSON allocates a tree."""
    depth=0;events=0;quoted=False;escaped=False
    for character in text:
        if quoted:
            if escaped:escaped=False
            elif character=='\\':escaped=True
            elif character=='"':quoted=False
        elif character=='"':quoted=True
        elif character in '{[':
            depth+=1;events+=1
            if depth>JSON_MAX_DEPTH:raise ValueError('JSON nesting bound exceeded')
        elif character in '}]':depth-=1
        elif character in ',:':events+=1
        if events>JSON_MAX_EVENTS:raise ValueError('JSON structural event bound exceeded')


def numeric_token_admitted(text):
    if len(text)>JSON_MAX_NUMBER_CHARS:raise ValueError('JSON numeric token bound exceeded')


def bounded_integer(text):
    numeric_token_admitted(text)
    return int(text)


def finite_float(text):
    numeric_token_admitted(text)
    value=float(text)
    if not math.isfinite(value):finite_constant(text)
    return value


def read_json(path, limit):
    """Admit bounded single-link JSON bound to the current named file throughout."""
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        info=os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size>limit:
            raise ValueError('Unclassified non-regular, multiply linked or oversized JSON')
        identity=lambda row:(row.st_dev,row.st_ino,row.st_mode,row.st_nlink,
                             row.st_size,row.st_mtime_ns,row.st_ctime_ns)
        if identity(info)!=identity(os.lstat(path)):
            raise ValueError('JSON named identity changed before inspection')
        with os.fdopen(fd,'rb',closefd=False) as stream:
            content=stream.read(limit+1)
        if len(content)>limit:raise ValueError('Unclassified oversized JSON')
        if len(content)!=info.st_size or identity(info)!=identity(os.fstat(fd)):
            raise ValueError('JSON changed during read')
        text=content.decode('utf-8')
        json_structure_admitted(text)
        value=json.loads(text,object_pairs_hook=unique_object,parse_constant=finite_constant,parse_float=finite_float,parse_int=bounded_integer)
        if identity(info)!=identity(os.fstat(fd)) or identity(info)!=identity(os.lstat(path)):
            raise ValueError('JSON changed during inspection')
        return value
    finally:
        os.close(fd)


def evidence_markers(value):
    pending = [value]
    while pending:
        item = pending.pop()
        if isinstance(item, dict):
            if item.get('artifact_class') in ('operational_job','fixture_session','retained_evidence','fixture_visual_output','package_proof','local_package_staging','release_artifact','authenticated_package','installed_package_predecessor','semantic_proof','retained_compiler_output','reference_environment','retained_contract_proof'):
                return True
            if set(item) & {'source_sha256', 'artifact_sha256', 'input_receipt_sha256', 'input_binary_sha256', 'backup_verified'}:
                return True
            pending.extend(item.values())
        elif isinstance(item, list):
            pending.extend(item)
    return False


# Selected disposable inventories are bounded before metadata reads or hashes.
BUILD_MAX_ENTRIES = 100000
BUILD_MAX_DEPTH = 64
BUILD_MAX_FILE_BYTES = 1024 * 1024 * 1024
BUILD_MAX_TOTAL_BYTES = 8 * 1024 * 1024 * 1024
BUILD_MAX_METADATA_BYTES = 64 * 1024 * 1024
BUILD_MAX_SECONDS = 120


def build_witness(info):
    return (info.st_dev,info.st_ino,info.st_mode,info.st_nlink,
            info.st_size,info.st_mtime_ns,info.st_ctime_ns)


def build_elapsed(start):
    if time.monotonic()-start>BUILD_MAX_SECONDS:
        raise ValueError('Build inventory sampled time bound exceeded')


def build_scan(build, extra_outputs=()):
    """Nofollow enumeration and complete declared byte preflight; no file reads."""
    build=Path(build);start=time.monotonic();witnesses={};entries={}
    total=0;metadata=0
    def add_file(path,info):
        nonlocal total,metadata
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1:
            raise ValueError('Unclassified non-regular or multiply linked build file: '+str(path))
        if info.st_size<0 or info.st_size>BUILD_MAX_FILE_BYTES:
            raise ValueError('Build file byte bound exceeded: '+str(path))
        total+=info.st_size
        if total>BUILD_MAX_TOTAL_BYTES:raise ValueError('Build aggregate byte budget exceeded')
        if path.suffix=='.json':metadata+=info.st_size
        if metadata>BUILD_MAX_METADATA_BYTES:raise ValueError('Build metadata byte budget exceeded')
    try:info=build.lstat()
    except FileNotFoundError:info=None
    witnesses[build]=build_witness(info) if info else None
    if info is not None and not stat.S_ISDIR(info.st_mode):
        raise ValueError('Build inventory root is not an admitted directory')
    pending=[(build,0)] if info is not None else []
    while pending:
        directory,depth=pending.pop();build_elapsed(start)
        fd=os.open(directory,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        try:
            if build_witness(os.fstat(fd))!=witnesses[directory]:
                raise ValueError('Build directory changed during inventory')
            with os.scandir(fd) as children:
                for child in children:
                    build_elapsed(start);path=directory/child.name;info=child.stat(follow_symlinks=False)
                    if len(entries)>=BUILD_MAX_ENTRIES:raise ValueError('Build inventory entry bound exceeded')
                    if depth+1>BUILD_MAX_DEPTH:raise ValueError('Build inventory depth bound exceeded')
                    if build_witness(path.lstat())!=build_witness(info):
                        raise ValueError('Build named entry changed during inventory')
                    entries[path]=info;witnesses[path]=build_witness(info)
                    if stat.S_ISDIR(info.st_mode):pending.append((path,depth+1))
                    else:add_file(path,info)
        finally:os.close(fd)
    for path in dict.fromkeys(extra_outputs):
        path=Path(path)
        if path in witnesses:continue
        build_elapsed(start)
        try:info=path.lstat()
        except FileNotFoundError:info=None
        witnesses[path]=build_witness(info) if info else None
        if info is not None:
            if len(entries)>=BUILD_MAX_ENTRIES:raise ValueError('Build inventory entry bound exceeded')
            entries[path]=info;add_file(path,info)
    build_elapsed(start)
    return {'entries':entries,'witnesses':witnesses,'metadata_bytes':metadata,'total_bytes':total}


RESERVATION_MAX_ENTRIES = 1000
RESERVATION_MAX_TOTAL_BYTES = 64 * 1024 * 1024
RESERVATION_MAX_SECONDS = 120
RESERVATION_MAX_ANCESTORS = 64


def reservation_scan(repo, build):
    """Stat-only preflight of every ancestor reservation; no receipt reads."""
    start=time.monotonic();witnesses={};receipts=[];total=0;ancestors=0
    def elapsed():
        if time.monotonic()-start>RESERVATION_MAX_SECONDS:
            raise ValueError('Package reservation sampled time bound exceeded')
    ancestor=build.parent
    while ancestor.is_relative_to(repo) and ancestor!=repo:
        elapsed();ancestors+=1
        if ancestors>RESERVATION_MAX_ANCESTORS:
            raise ValueError('Package reservation ancestor bound exceeded')
        directory=ancestor/'.package-reservations'
        try:info=directory.lstat()
        except FileNotFoundError:info=None
        witnesses[directory]=build_witness(info) if info else None
        if info is not None:
            if not stat.S_ISDIR(info.st_mode):
                raise ValueError('Unclassified package reservation directory')
            fd=os.open(directory,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
            try:
                if build_witness(os.fstat(fd))!=witnesses[directory]:
                    raise ValueError('Package reservation directory changed during inspection')
                with os.scandir(fd) as children:
                    for child in children:
                        elapsed();path=directory/child.name;info=child.stat(follow_symlinks=False)
                        if len(receipts)>=RESERVATION_MAX_ENTRIES:
                            raise ValueError('Package reservation entry bound exceeded')
                        if (path.suffix!='.json' or not stat.S_ISREG(info.st_mode)
                                or info.st_nlink!=1 or info.st_size>1024*1024):
                            raise ValueError('Unclassified package reservation entry')
                        total+=info.st_size
                        if total>RESERVATION_MAX_TOTAL_BYTES:
                            raise ValueError('Package reservation aggregate byte budget exceeded')
                        if build_witness(path.lstat())!=build_witness(info):
                            raise ValueError('Package reservation named entry changed during inspection')
                        witnesses[path]=build_witness(info);receipts.append(path)
            finally:os.close(fd)
        ancestor=ancestor.parent
    elapsed()
    return {'witnesses':witnesses,'receipts':sorted(receipts)}


def reservation_output(repo, receipt, row):
    """Admit the two current producer schemas and their exact output identity."""
    invalid='Invalid package output reservation'
    common={'schema','artifact_class','attempt_id','output','state'}
    if not isinstance(row,dict):raise ValueError(invalid)
    state=row.get('state')
    if state=='transaction_reserved':expected=common
    elif state=='fresh_reserved_attempt':expected=common|{'root','release_authority_granted'}
    else:raise ValueError(invalid+' state')
    if set(row)!=expected or row.get('schema')!='physics_sim_artifact_owner_v1' or row.get('artifact_class')!='local_package_staging':
        raise ValueError(invalid+' schema')
    attempt=row.get('attempt_id')
    if not isinstance(attempt,str) or len(attempt)!=36:raise ValueError(invalid+' attempt')
    try:identity=uuid.UUID(attempt)
    except ValueError:raise ValueError(invalid+' attempt')
    if identity.version!=4 or str(identity)!=attempt:raise ValueError(invalid+' attempt')
    root=receipt.parent.parent
    if not (root.is_relative_to(repo/'build') and root!=repo/'build'
            or root==repo/'dist' or root.is_relative_to(repo/'dist')):
        raise ValueError(invalid+' namespace')
    if state=='fresh_reserved_attempt':
        if row['root']!=str(root) or row['release_authority_granted'] is not False:
            raise ValueError(invalid+' root or authority')
    value=row.get('output')
    if not isinstance(value,str) or not value or len(value)>4096 or any(ord(c)<32 for c in value):
        raise ValueError(invalid+' output')
    output=Path(value)
    if not output.is_absolute() or str(output)!=value or '..' in output.parts or not output.is_relative_to(root):
        raise ValueError(invalid+' output namespace')
    if state=='transaction_reserved' and output==root:raise ValueError(invalid+' output root')
    current=output
    while current!=repo:
        if current.is_symlink():raise ValueError(invalid+' linked output')
        current=current.parent
    if receipt.name!=hashlib.sha256(value.encode()).hexdigest()+'.json':
        raise ValueError(invalid+' filename identity')
    return output


def check(repo, build, protected):
    repo = repo.resolve()
    build = build.resolve()
    if build == repo or repo.is_relative_to(build) or not build.is_relative_to(repo):
        raise ValueError('Clean build root must be a strict descendant of this checkout')
    if any(part in HELD_DIRECTORIES for part in build.relative_to(repo).parts):
        raise ValueError('Selected root is retained/operational storage: '+str(build))
    for path in protected:
        path = path.resolve()
        if path == build or path.is_relative_to(build) or build.is_relative_to(path):
            raise ValueError('Clean root overlaps protected storage: '+str(path))
    # Package attempt reservations live outside shipped payloads. Protect a
    # selected descendant as well as cleaning a whole build root.
    reservations=reservation_scan(repo,build)
    reservation_start=time.monotonic()
    for receipt in reservations['receipts']:
        if time.monotonic()-reservation_start>RESERVATION_MAX_SECONDS:
            raise ValueError('Package reservation sampled time bound exceeded')
        try:row=read_json(receipt,1024*1024)
        except (ValueError,OSError,RecursionError):raise ValueError('Unclassified package reservation')
        output=reservation_output(repo,receipt,row)
        if output==build or output.is_relative_to(build) or build.is_relative_to(output):
            raise ValueError('Clean root overlaps reserved package output: '+str(output))
    # Legacy experiment runners left frozen inputs/fields and receipts in build.
    # Refuse rather than guessing these can be regenerated and silently deleting.
    if build.exists():
        selected=build_scan(build)
        for path in selected['entries']:
            if path.is_symlink():
                raise ValueError('Unclassified symlink under clean root: '+str(path))
            mode = path.stat().st_mode
            if not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise ValueError('Unclassified non-regular file under clean root: '+str(path))
            if (path.name in ('bundle_manifest.json', 'field.bin', '.physics-sim-headless-owner', '.physics-sim-job-metadata.lock', '.physics-sim-job-operation.lock') or (path.name.startswith('.headless-sidecar-') and path.name.endswith('.pending')) or path.suffix in ('.npz', '.npy')
                    or path.is_dir() and path.name in HELD_DIRECTORIES):
                raise ValueError('Retained evidence under clean root: '+str(path))
            if path.suffix == '.json':
                try:
                    row = read_json(path,16*1024*1024)
                except (ValueError, OSError, RecursionError):
                    raise ValueError('Unclassified JSON under clean root: '+str(path))
                if evidence_markers(row):
                    raise ValueError('Legacy retained receipt under clean root: '+str(path))
    if reservation_scan(repo,build)['witnesses']!=reservations['witnesses']:
        raise ValueError('Package reservations changed during cleanup inspection')
    if time.monotonic()-reservation_start>RESERVATION_MAX_SECONDS:
        raise ValueError('Package reservation sampled time bound exceeded')
    return reservations['witnesses']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('build-root', 'experiment-root', 'tools-root', 'test-root'):
        parser.add_argument('--'+option, type=Path, required=True)
    args = parser.parse_args()
    check(Path.cwd(), args.build_root, [args.experiment_root, args.tools_root, args.test_root,
                                      Path('src'), Path('include'), Path('scripts'), Path('tests'), Path('docs'), Path('make'),
                                      Path('.git'), Path('.agents'), Path('.codex'), Path('config'),
                                      Path('third_party'), Path('data'), Path('export'), Path('dist')])


if __name__ == '__main__':
    main()
