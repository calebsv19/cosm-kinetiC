"""Exact disposable-output receipts produced by successful local build writers.

Receipts survive clean. They are ownership evidence, not arbitrary-file adoption
or proof that a compiler's output is numerically correct.
"""
import hashlib
import json
import os
from pathlib import Path
import stat
import tempfile
import time
import check_clean_root as cleanup_policy

from check_clean_root import read_json

ROOT_EXECUTABLES = frozenset((
    'physics_sim', 'physics_sim_headless', 'physics_sim_job_runner',
    'physics_sim_session_worker', 'physics_trace_tool', 'vf2d_pack_tool',
    'vf2d_dataset_tool', 'runtime_scene_emitter_diag_tool', 'shape_sanity_tool',
    'shape_mask_tool', 'shape_asset_tool',
))


def checked_path(path, repo):
    path = Path(os.path.abspath(path))
    original_repo = Path(os.path.abspath(repo))
    repo = repo.resolve()
    if path.is_relative_to(original_repo):
        path = repo/path.relative_to(original_repo)
    if not path.is_relative_to(repo) or path == repo:
        raise ValueError('Build receipt path must be inside checkout')
    current = path
    while current != repo:
        if current.is_symlink():
            raise ValueError('Build receipt refuses symlink component: '+str(current))
        current = current.parent
    return path


class BoundedHashInput:
    """Expose only the admitted original bytes and one overflow observation."""
    def __init__(self,stream,size):self.stream=stream;self.remaining=size
    def readable(self):return True
    def readinto(self,buffer):
        if not self.remaining:
            if self.stream.read(1):raise ValueError('Disposable output grew during inspection')
            return 0
        target=memoryview(buffer)[:min(len(buffer),self.remaining)]
        count=self.stream.readinto(target)
        if not count or count>len(target):raise ValueError('Disposable output truncated during inspection')
        self.remaining-=count
        return count


def fingerprint(path, *, require_single_link=False, max_bytes=None, expected_identity=None):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or (require_single_link and before.st_nlink != 1):
            raise ValueError('Inspected file must be regular with required link ownership')
        if (require_single_link and before.st_size>cleanup_policy.BUILD_MAX_FILE_BYTES) or (max_bytes is not None and before.st_size>max_bytes):
            raise ValueError('Build file byte bound exceeded')
        identity = lambda row: [row.st_dev, row.st_ino, row.st_mode, row.st_size, row.st_mtime_ns, row.st_ctime_ns]
        if expected_identity is not None and identity(before)!=expected_identity:
            raise ValueError('Dependency input changed after budget preflight')
        named = os.lstat(path)
        if named.st_nlink != before.st_nlink or identity(named) != identity(before):
            raise ValueError('Disposable output named identity changed before inspection')
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            reader=BoundedHashInput(stream,before.st_size) if require_single_link or max_bytes is not None else stream
            digest = hashlib.file_digest(reader, 'sha256').hexdigest()
        after = os.fstat(fd)
        named = os.lstat(path)
        if (after.st_nlink != before.st_nlink or named.st_nlink != after.st_nlink or
                identity(before) != identity(after) or identity(after) != identity(named)):
            raise ValueError('Disposable output changed during inspection')
        return {'identity': identity(after), 'sha256': digest}
    finally:
        os.close(fd)


def dependency_tokens(line):
    if line.startswith('\t') or any(c in line for c in '$;|%='):
        raise ValueError('Generated dependency contains executable or unsupported Make syntax')
    tokens=[];word='';escaped=False
    for c in line:
        if escaped:
            if c not in ' \t#:\\':raise ValueError('Unsupported generated dependency escape')
            word+=c;escaped=False
        elif c=='\\':escaped=True
        elif c=='#':raise ValueError('Unsupported generated dependency comment')
        elif c==':':
            if word:tokens.append(word);word=''
            tokens.append(None)
        elif c.isspace():
            if word:tokens.append(word);word=''
        else:word+=c
    if escaped:raise ValueError('Truncated generated dependency escape')
    if word:tokens.append(word)
    return tokens


def dependency_syntax(repo,path,text):
    if '\x00' in text or any(ord(c)<32 and c not in '\n\t' for c in text):
        raise ValueError('Generated dependency control character')
    lines=text.replace('\\\n',' ').splitlines();rules=[]
    for line in lines:
        if not line.strip():continue
        tokens=dependency_tokens(line)
        if tokens.count(None)!=1 or tokens.index(None)!=1:
            raise ValueError('Invalid generated dependency rule')
        rules.append((tokens[0],tokens[2:]))
    absolute=lambda value:Path(os.path.abspath(repo/value))
    if not rules or absolute(rules[0][0])!=path.with_suffix('.o') or not rules[0][1]:
        raise ValueError('Generated dependency object identity mismatch')
    dependencies=set(rules[0][1])
    for target,values in rules[1:]:
        if values or target not in dependencies:
            raise ValueError('Unclassified generated dependency rule')
        # GCC can repeat a dependency and its identical empty -MP rule.
        # These rules add no input or recipe beyond the first object rule.
    return sorted({absolute(value) for value in dependencies})



INPUT_PASS_MAX_ENTRIES = 100000
INPUT_PASS_MAX_HASH_BYTES = 1024 * 1024 * 1024


def dependency_input_snapshot(repo,path,text, input_budget=None, input_witnesses=None):
    inputs=dependency_syntax(repo,path,text)
    if len(inputs)>10000:raise ValueError('Dependency input count bound exceeded')
    selected=[];total=0
    if input_budget is None:input_budget={'entries':0,'hash_bytes':0}
    for original in inputs:
        resolved=original.resolve(strict=True);info=resolved.stat()
        if not stat.S_ISREG(info.st_mode) or info.st_size>64*1024*1024:
            raise ValueError('Dependency input file bound exceeded')
        total+=info.st_size
        if total>256*1024*1024:raise ValueError('Dependency input aggregate byte bound exceeded')
        input_budget['entries']+=1;input_budget['hash_bytes']+=2*info.st_size
        if input_budget['entries']>INPUT_PASS_MAX_ENTRIES or input_budget['hash_bytes']>INPUT_PASS_MAX_HASH_BYTES:
            raise ValueError('Dependency input complete-pass budget exceeded')
        expected=[info.st_dev,info.st_ino,info.st_mode,info.st_size,info.st_mtime_ns,info.st_ctime_ns]
        selected.append((original,resolved,expected))
    rows=[];witnesses=[]
    for original,resolved,expected in selected:
        row=fingerprint(resolved,max_bytes=64*1024*1024,expected_identity=expected)
        if original.resolve(strict=True)!=resolved:raise ValueError('Dependency input resolution changed')
        rows.append([str(original),str(resolved),row['sha256']]);witnesses.append((original,resolved,row))
    for original,resolved,row in witnesses:
        if original.resolve(strict=True)!=resolved or fingerprint(resolved,max_bytes=64*1024*1024,expected_identity=row['identity'])!=row:
            raise ValueError('Dependency input changed during complete snapshot')
    if input_witnesses is not None:input_witnesses.extend(witnesses)
    digest=hashlib.sha256(json.dumps(rows,separators=(',',':'),ensure_ascii=True).encode()).hexdigest()
    return {'schema':'physics_sim_dependency_inputs_v1','scope':'postcompiler_observation',
            'count':len(rows),'sha256':digest}


def receipt_path(repo, output):
    output = checked_path(output, repo)
    repo = repo.resolve()
    key = hashlib.sha256(str(output.relative_to(repo)).encode()).hexdigest()
    return checked_path(repo/'tmp/build-output-ownership'/key[:2]/(key+'.json'), repo)


def record(repo, output, producer, *, input_snapshot=None, dependency_sha256=None, link_inputs_sha256=None):
    output = checked_path(output, repo)
    repo = repo.resolve()
    if (not output.is_relative_to(repo/'build')
            and not (producer == 'compiler' and output.parent == repo and output.name in ROOT_EXECUTABLES)):
        raise ValueError('Undeclared disposable output namespace: '+str(output))
    if producer not in ('compiler', 'dependency', 'configuration', 'linker-inputs'):
        raise ValueError('Unknown disposable-output producer')
    row = {'schema': 'physics_sim_disposable_output_v1',
           'path': str(output.relative_to(repo)), 'producer': producer,
           **fingerprint(output, require_single_link=True)}
    if input_snapshot is not None:row['input_snapshot']=input_snapshot
    if dependency_sha256 is not None:row['dependency_sha256']=dependency_sha256
    if link_inputs_sha256 is not None:row['link_inputs_sha256']=link_inputs_sha256
    receipt = receipt_path(repo, output)
    receipt.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.ownership-', dir=receipt.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(row, stream, sort_keys=True, allow_nan=False)
            stream.write('\n'); stream.flush(); os.fsync(stream.fileno())
        checked_path(receipt, repo)
        os.replace(temporary, receipt)
    finally:
        Path(temporary).unlink(missing_ok=True)
    return row


def verified(repo, output):
    output = checked_path(output, repo)
    repo = repo.resolve()
    receipt = receipt_path(repo, output)
    try:
        row = read_json(receipt, 16384)
    except FileNotFoundError:
        raise ValueError('Unknown disposable output; cleanup held: '+str(output))
    if (not isinstance(row, dict) or row.get('schema') != 'physics_sim_disposable_output_v1'
            or row.get('path') != str(output.relative_to(repo))
            or row.get('producer') not in ('compiler', 'dependency', 'configuration', 'linker-inputs')):
        raise ValueError('Invalid disposable-output receipt: '+str(output))
    actual = fingerprint(output, require_single_link=True)
    if any(row.get(key) != value for key, value in actual.items()):
        raise ValueError('Changed disposable output; cleanup held: '+str(output))
    return {'path': str(output), **actual}


def inventory(repo, build, extra_outputs=()):
    start=time.monotonic()
    selected=cleanup_policy.build_scan(build,extra_outputs)
    files=[];directories=set();receipts={};metadata=selected['metadata_bytes']
    outputs=[]
    for path,info in sorted(selected['entries'].items()):
        checked_path(path,repo)
        if stat.S_ISDIR(info.st_mode):directories.add(path);continue
        outputs.append(path);receipt=receipt_path(repo,path)
        cleanup_policy.build_elapsed(start)
        try:info=receipt.lstat()
        except FileNotFoundError:raise ValueError('Unknown disposable output; cleanup held: '+str(path))
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size>16384:
            raise ValueError('Unclassified disposable-output receipt: '+str(receipt))
        metadata+=info.st_size
        if metadata>cleanup_policy.BUILD_MAX_METADATA_BYTES:
            raise ValueError('Build metadata byte budget exceeded')
        receipts[receipt]=cleanup_policy.build_witness(info)
    # All selected outputs and receipt sizes are admitted before the first hash.
    for path in outputs:
        cleanup_policy.build_elapsed(start);files.append(verified(repo,path))
    observed=cleanup_policy.build_scan(build,extra_outputs)
    if observed['witnesses']!=selected['witnesses']:
        raise ValueError('Build inventory changed during inspection')
    for path,witness in receipts.items():
        if cleanup_policy.build_witness(path.lstat())!=witness:
            raise ValueError('Build ownership receipt changed during inventory')
    cleanup_policy.build_elapsed(start)
    owned_parents=set()
    for row in files:
        path=Path(row['path'])
        if not path.is_relative_to(build):continue
        parent=path.parent
        while parent!=build:
            owned_parents.add(parent);parent=parent.parent
    unknown=directories-owned_parents
    if unknown:raise ValueError('Unknown build directory; cleanup held: '+str(sorted(unknown)[0]))
    return files


class LinkInputsChanged(ValueError):
    """A valid prior link selection no longer names the current inputs."""


def linker_dependency_records(path, cwd, expected_output=None):
    """Admit Darwin dependency_info records as bounded data, never Make syntax."""
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1 or before.st_size > 1024*1024:
            raise ValueError('Linker dependency trace file bound exceeded')
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            data = stream.read(before.st_size+1)
        if (len(data) != before.st_size or cleanup_policy.build_witness(os.fstat(fd)) != cleanup_policy.build_witness(before)
                or cleanup_policy.build_witness(path.lstat()) != cleanup_policy.build_witness(before)):
            raise ValueError('Linker dependency trace changed during read')
    finally:
        os.close(fd)
    offset = 0; version = None; selected = set(); absent = set(); outputs = set(); count = 0
    while offset < len(data):
        opcode = data[offset]; end = data.find(b'\0', offset+1)
        if end < 0 or end-offset > 4097:
            raise ValueError('Malformed linker dependency record')
        value = data[offset+1:end].decode('utf-8'); offset = end+1; count += 1
        if count > 10000 or not value:
            raise ValueError('Linker dependency record count/value bound exceeded')
        if opcode == 0:
            if version is not None or count != 1:
                raise ValueError('Duplicate or misplaced linker version record')
            version = value
            continue
        if version is None or opcode not in (0x10, 0x11, 0x40):
            raise ValueError('Unknown linker dependency record opcode')
        if any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError('Control characters in linker input path')
        absolute = str(Path(os.path.abspath(cwd/value)))
        if opcode == 0x10: selected.add(absolute)
        elif opcode == 0x11: absent.add(absolute)
        else: outputs.add(absolute)
    if expected_output is not None and outputs != {str(Path(os.path.abspath(expected_output)))}:
        raise ValueError('Linker trace output does not match the staged publication')
    if version is None or not selected or selected & absent:
        raise ValueError('Incomplete or contradictory linker dependency trace')
    return {'schema':'physics_sim_link_selection_v1', 'linker_version':version,
            'selected':sorted(selected), 'absent':sorted(absent)}


def validate_link_selection(selection):
    if (not isinstance(selection, dict) or set(selection) != {'schema','linker_version','selected','absent'}
            or selection['schema'] != 'physics_sim_link_selection_v1'
            or not isinstance(selection['linker_version'], str) or not 1 <= len(selection['linker_version'].encode()) <= 4096):
        raise ValueError('Invalid linker selection metadata')
    for key in ('selected', 'absent'):
        rows = selection[key]
        if not isinstance(rows, list) or len(rows) > 10000:
            raise ValueError('Linker selection count bound exceeded')
        for name in rows:
            if (not isinstance(name,str) or not name.startswith('/') or len(name.encode()) > 4096
                    or str(Path(os.path.abspath(name))) != name or any(ord(c)<32 or ord(c)==127 for c in name)):
                raise ValueError('Invalid absolute linker selection path')
        if rows != sorted(set(rows)):
            raise ValueError('Linker selection must be sorted and unique')
    if len(selection['selected'])+len(selection['absent'])>10000:
        raise ValueError('Linker selection aggregate count bound exceeded')
    if not selection['selected'] or set(selection['selected']) & set(selection['absent']):
        raise ValueError('Contradictory linker selection')


def link_input_snapshot(selection, budget=None, witnesses=None):
    validate_link_selection(selection)
    if budget is None: budget = {'entries':0,'hash_bytes':0}
    selected = []; total = 0; absent = []
    for name in selection['selected']:
        original = Path(name)
        try: resolved = original.resolve(strict=True); info = resolved.stat()
        except FileNotFoundError as error: raise LinkInputsChanged('Selected linker input disappeared') from error
        if not stat.S_ISREG(info.st_mode) or info.st_size > 64*1024*1024:
            raise ValueError('Linker input file bound exceeded')
        total += info.st_size
        budget['entries'] += 1; budget['hash_bytes'] += 2*info.st_size
        if total > 256*1024*1024 or budget['entries'] > INPUT_PASS_MAX_ENTRIES or budget['hash_bytes'] > INPUT_PASS_MAX_HASH_BYTES:
            raise ValueError('Linker input aggregate budget exceeded')
        expected = [info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns, info.st_ctime_ns]
        selected.append((original,resolved,expected))
    for name in selection['absent']:
        original = Path(name); resolved = original.resolve()
        if os.path.lexists(original): raise LinkInputsChanged('Previously absent linker search candidate appeared')
        budget['entries'] += 1
        if budget['entries'] > INPUT_PASS_MAX_ENTRIES: raise ValueError('Linker input aggregate entry budget exceeded')
        absent.append((original,resolved))
    rows = []; observed = []
    for original,resolved,expected in selected:
        row = fingerprint(resolved,max_bytes=64*1024*1024,expected_identity=expected)
        if original.resolve(strict=True) != resolved: raise LinkInputsChanged('Linker input resolution changed')
        rows.append([str(original),str(resolved),row['sha256']]); observed.append((original,resolved,row))
    for original,resolved,row in observed:
        if original.resolve(strict=True) != resolved or fingerprint(resolved,max_bytes=64*1024*1024,expected_identity=row['identity']) != row:
            raise LinkInputsChanged('Linker input changed during snapshot')
    for original,resolved in absent:
        if os.path.lexists(original) or original.resolve() != resolved:
            raise LinkInputsChanged('Linker absent input changed during snapshot')
    payload = [selection['linker_version'],rows,[[str(a),str(b)] for a,b in absent]]
    digest = hashlib.sha256(json.dumps(payload,separators=(',',':'),ensure_ascii=True).encode()).hexdigest()
    if witnesses is not None: witnesses.extend(observed); witnesses.extend(absent)
    return {'schema':'physics_sim_link_inputs_v1','scope':'linker_boundary_observation',
            'selected_count':len(selected),'absent_count':len(absent),'sha256':digest}
