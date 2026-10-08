"""Standard-library-only bounded local run supervision and immutable JSON writes."""
import fcntl
import hashlib
from contextlib import ExitStack, contextmanager
from types import SimpleNamespace
import math
import re
import select
import sys
import threading
import json
import os
import signal
import subprocess
import time
from pathlib import Path
import stat
import uuid

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    with path.open('x') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


class ExecutionFailure(ValueError):
    """Command failed with an observed exit status."""
    def __init__(self, tag, exit_code):
        super().__init__(tag+' exited '+str(exit_code));self.exit_code=exit_code

class IncompleteTeardown(ValueError):
    """Owned cleanup/result unverified; retain an unsealed attempt."""


def anchor_child(result_fd, lifeline_fd, descriptors, command, rss_cap):
    for number in (signal.SIGINT, signal.SIGTERM):signal.signal(number, lambda *_: None)
    child = None
    peak = 0
    failure = None
    code = None
    try:
        child = subprocess.Popen(command, pass_fds=descriptors)
        # The anchor is the only waiter. WNOWAIT keeps the command PID owned
        # throughout RSS sampling, including exit during the ps invocation.
        while os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
            if select.select([lifeline_fd], [], [], .01)[0] and not os.read(lifeline_fd, 1):
                os.killpg(os.getpgrp(), signal.SIGKILL)
                raise RuntimeError('CFD anchor self-cleanup failed')
            sample = subprocess.run(['/bin/ps','-o','rss=','-p',str(child.pid)],
                                    capture_output=True, text=True, timeout=1)
            text = sample.stdout.strip()
            terminal = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None
            if sample.returncode == 0 and re.fullmatch('[0-9]{1,15}', text):
                peak = max(peak, int(text)*1024)
                if peak > rss_cap:failure = 'RSS cap';break
            elif not terminal:
                failure = 'RSS observation unverified';break
            if not terminal:time.sleep(.05)
        if failure and os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
            # Only this anchor waits for its unreaped direct child. Group
            # cleanup remains the outer parent's anchored responsibility.
            os.kill(child.pid, signal.SIGKILL)
        code = child.wait(timeout=5)
    except (OSError, subprocess.TimeoutExpired) as error:
        failure = 'RSS/command observation failed: ' + type(error).__name__
        if child is not None:
            try:
                if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
                    os.kill(child.pid, signal.SIGKILL)
                code = child.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):pass
    payload = {'exit_code':code,'peak_rss':peak,'failure':failure}
    os.write(result_fd,(json.dumps(payload,separators=(',',':'))+'\n').encode('ascii'));os.close(result_fd)
    while os.read(lifeline_fd,1):pass
    os.killpg(os.getpgrp(),signal.SIGKILL)
    raise RuntimeError('CFD anchor self-cleanup failed')


def signal_owned_group(child,number):
    if child.returncode is not None:raise ValueError('Retained-command anchor already reaped')
    observed=os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT)
    if observed is None:
        try:
            if os.getpgid(child.pid)!=child.pid:raise ValueError('Retained-command group identity changed')
        except ProcessLookupError:
            if os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:raise
    try:os.killpg(child.pid,number);return True
    except ProcessLookupError:return False


def execute(command,directory,tag,wall_cap,rss_cap,log_cap=67108864,*,cwd=None,env=None,combined_log=None):
    directory=Path(directory)
    cwd=directory if cwd is None else cwd
    if combined_log is not None and Path(combined_log).parent != directory:
        raise ValueError('Combined CFD log must be an immediate output artifact')
    descriptors=reference_owner_descriptors(directory)
    if (type(wall_cap) not in (int,float) or not math.isfinite(wall_cap) or not 0<wall_cap<=86400 or
        type(log_cap) is not int or not 1<=log_cap<=1073741824 or
        type(rss_cap) is not int or not 1<=rss_cap<=1099511627776 or
        not isinstance(tag,str) or not re.fullmatch('[a-zA-Z0-9][a-zA-Z0-9_-]{0,95}',tag)):
        raise ValueError('Invalid CFD command tag/time/RSS/log bounds')
    if not all(hasattr(os,name) for name in ('waitid','P_PID','WEXITED','WNOHANG','WNOWAIT')):
        raise ValueError('Retained command requires non-reaping child observation')
    command=[os.fsdecode(value) for value in command]
    started=time.monotonic();child=None;signals=[];handlers={};teardown_errors=[]
    result_read,result_write=os.pipe();life_read,life_write=os.pipe();os.set_blocking(result_read,False)
    result_bytes=b'';observed=False;exit_code=None;launch_errno=None;row={};anchored=False;reaped=False;peak=0;resource_failure=None
    def read_result():
        nonlocal result_bytes,observed,exit_code,peak,resource_failure
        try:chunk=os.read(result_read,513)
        except BlockingIOError:return
        if not chunk:raise ValueError('CFD result pipe closed prematurely')
        result_bytes+=chunk
        if len(result_bytes)>512:raise ValueError('CFD result protocol bound')
        if b'\n' in result_bytes:
            if not result_bytes.endswith(b'\n') or result_bytes.count(b'\n')!=1:
                raise ValueError('Malformed CFD result framing')
            def unique(items):
                out={}
                for key,value in items:
                    if key in out:raise ValueError('Duplicate CFD result field')
                    out[key]=value
                return out
            value=json.loads(result_bytes,object_pairs_hook=unique)
            if not isinstance(value,dict) or set(value)!= {'exit_code','peak_rss','failure'}:
                raise ValueError('Malformed CFD result fields')
            exit_code=value['exit_code'];peak=value['peak_rss'];resource_failure=value['failure']
            if (type(exit_code) is not int or not -128<=exit_code<=255 or
                type(peak) is not int or not 0<=peak<=1099511627776000 or
                (resource_failure is not None and (not isinstance(resource_failure,str) or len(resource_failure)>128))):
                raise ValueError('Unverified CFD command result')
            observed=True
    def forward(number,frame):
        signals.append(number)
        if child is not None:
            try:signal_owned_group(child,number)
            except (OSError,ValueError) as error:teardown_errors.append(str(error))
    try:
        if threading.current_thread() is threading.main_thread():
            for number in (signal.SIGINT,signal.SIGTERM):handlers[number]=signal.signal(number,forward)
        with ExitStack() as logs:
            out=logs.enter_context((Path(combined_log) if combined_log is not None else directory/(tag+'.stdout')).open('xb'))
            err=out if combined_log is not None else logs.enter_context((directory/(tag+'.stderr')).open('xb'))
            child=subprocess.Popen([sys.executable,'-B',str(Path(__file__).resolve()),'--cfd-anchor',
                str(result_write),str(life_read),json.dumps(list(descriptors)),json.dumps(command),str(rss_cap)],
                cwd=cwd,env=env,stdout=out,stderr=err,start_new_session=True,pass_fds=(*descriptors,result_write,life_read))
            os.close(result_write);result_write=None;os.close(life_read);life_read=None
            while not observed:
                if signals:raise ValueError('Contract interrupted by signal '+str(signals[-1]))
                if time.monotonic()-started>wall_cap:raise ValueError('Contract exceeded wall cap')
                if sum(os.fstat(stream.fileno()).st_size for stream in set((out,err)))>log_cap:
                    raise ValueError('Contract exceeded retained log cap')
                if os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                    raise ValueError('Retained command anchor exited before handoff')
                if select.select([result_read],[],[],.05)[0]:read_result()
            if signals:raise ValueError('Contract interrupted by signal '+str(signals[-1]))
            if sum(os.fstat(stream.fileno()).st_size for stream in set((out,err)))>log_cap:
                raise ValueError('Contract exceeded retained log cap')
            if resource_failure:raise ValueError(tag+': '+resource_failure)
            if exit_code:raise ExecutionFailure(tag,exit_code)
            for stream in set((out,err)):stream.flush();os.fsync(stream.fileno())
            row={'command':command,'exit_code':0,'wall_s':time.monotonic()-started,'peak_sampled_child_rss_bytes':peak,'rss_scope':'Direct command only; descendants unmeasured','reference_ownership_descriptor_count':len(descriptors)}
            return row
    finally:
        prior_error=sys.exc_info()[1]
        if child is not None:
            if not observed:
                try:signal_owned_group(child,signal.SIGTERM)
                except (OSError,ValueError) as error:teardown_errors.append(str(error))
                deadline=time.monotonic()+5
                while not observed and time.monotonic()<deadline:
                    try:
                        if select.select([result_read],[],[],.05)[0]:read_result()
                        if os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:break
                    except (OSError,ValueError) as error:teardown_errors.append(str(error));break
            try:signal_owned_group(child,signal.SIGKILL);anchored=True
            except (OSError,ValueError) as error:teardown_errors.append(str(error))
            os.close(life_write);life_write=None
            try:
                os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT)
                child.wait(timeout=5);reaped=True
            except (OSError,subprocess.TimeoutExpired) as error:teardown_errors.append(str(error))
            if not observed:teardown_errors.append('Command result not observed before cleanup')
        for fd in (result_read,result_write,life_read,life_write):
            if fd is not None:os.close(fd)
        for number,handler in handlers.items():signal.signal(number,handler)
        scope=('Observed command outcome and reaped direct anchor; complete descendant termination unverified' if observed and reaped and anchored and not teardown_errors else 'Command outcome/anchor cleanup unverified; retain all evidence')
        row.update(direct_child_reaped=reaped,group_cleanup_identity_anchored=anchored,
                   terminal_processes_verified=observed and reaped and anchored and not teardown_errors,
                   terminal_verification_scope=scope,all_external_descendants_verified_terminal=False)
        if prior_error is not None:
            prior_error.direct_child_reaped=reaped
            prior_error.group_cleanup_identity_anchored=anchored
            prior_error.terminal_processes_verified=observed and reaped and anchored and not teardown_errors
            prior_error.exit_code=exit_code
            prior_error.peak_sampled_child_rss_bytes=peak
            prior_error.rss_scope='Direct command only; descendants unmeasured'
            prior_error.terminal_verification_scope=scope
            prior_error.all_external_descendants_verified_terminal=False
        if teardown_errors:raise IncompleteTeardown('Contract teardown unverified: '+str(prior_error or '')+'; '+'; '.join(teardown_errors))



def reference_execute(command, directory, log_path, wall_cap, rss_cap, *, cwd, env):
    """Preserve reference numerical outcomes and combined log chronology."""
    try:
        row=execute(command,directory,'reference',wall_cap,rss_cap,cwd=cwd,env=env,combined_log=log_path)
        code=row['exit_code'];reason=None;peak=row['peak_sampled_child_rss_bytes']
    except ExecutionFailure as error:
        code=error.exit_code;reason=None;peak=error.peak_sampled_child_rss_bytes
        row={'terminal_verification_scope':error.terminal_verification_scope,
             'all_external_descendants_verified_terminal':False,'rss_scope':error.rss_scope,'direct_child_reaped':error.direct_child_reaped,
             'group_cleanup_identity_anchored':error.group_cleanup_identity_anchored,
             'terminal_processes_verified':error.terminal_processes_verified}
    except (ValueError,OSError) as error:
        code=getattr(error,'exit_code',None);peak=getattr(error,'peak_sampled_child_rss_bytes',0)
        reason=('reference resource cap: '+str(error) if any(word in str(error) for word in ('RSS cap','wall cap','log cap')) else 'reference supervision hold: '+str(error))
        row={'terminal_verification_scope':getattr(error,'terminal_verification_scope','Command/cleanup unverified'),
             'all_external_descendants_verified_terminal':False,'rss_scope':'Direct command only; descendants unmeasured'}
    return SimpleNamespace(returncode=code),reason,peak,row


def compile_probe(command, directory, tag, wall_cap, rss_cap, *, cwd=None):
    """Publish a fresh retained probe without overwriting a prior binary.

    Candidates, failures and command logs stay in the caller's retained bundle;
    these are evidence outputs, never disposable-output registrations.
    """
    directory = Path(directory).resolve()
    require(command.count('-o') == 1, 'Retained compiler requires one -o')
    index = command.index('-o')+1
    require(index < len(command), 'Missing retained compiler output')
    output = Path(command[index])
    output = output if output.is_absolute() else directory/output
    require(output.parent == directory and output.suffix not in ('.c', '.h'),
            'Retained compiler output must be an immediate bundle artifact')
    require(not output.exists() and not output.is_symlink(), 'Retained compiler output already exists')
    attempts = directory/'.compiler-attempts'
    require(not attempts.is_symlink(), 'Retained compiler attempt root is a symlink')
    attempts.mkdir(exist_ok=True)
    attempt = attempts/uuid.uuid4().hex
    attempt.mkdir()
    candidate = attempt/output.name
    argv = list(command); argv[index] = str(candidate)
    if '-dynamiclib' in argv and '-install_name' not in argv:
        argv += ['-install_name',str(output)]
    request = {'schema': 'physics_sim_retained_compile_v1', 'artifact_class': 'retained_compiler_output',
               'requested_command': command, 'executed_command': argv,
               'output': str(output.relative_to(directory)), 'status': 'running'}
    save(attempt/'request.json', request)
    try:
        result = execute(argv, directory, tag, wall_cap, rss_cap, cwd=cwd)
        require(candidate.is_file() and not candidate.is_symlink()
                and stat.S_ISREG(candidate.stat().st_mode), 'Compiler succeeded without regular probe')
        digest = sha(candidate)
        with candidate.open('rb') as stream: os.fsync(stream.fileno())
        # Same-filesystem hard link is an atomic no-replace publication. Keep
        # the candidate for diagnosis and never unlink a predecessor.
        os.link(candidate, output, follow_symlinks=False)
        require(sha(output) == digest, 'Retained compiler publication drift')
        request.update(status='passed', output_sha256=digest, supervision={key:result[key] for key in ('direct_child_reaped','group_cleanup_identity_anchored','terminal_verification_scope','all_external_descendants_verified_terminal','peak_sampled_child_rss_bytes','rss_scope')})
        save(attempt/'receipt.json', request)
        return {**result, 'requested_command': command, 'output_sha256': digest,
                'compile_receipt': str((attempt/'receipt.json').relative_to(directory))}
    except BaseException as error:
        if not (attempt/'receipt.json').exists():
            request.update(status='failed', failure=str(error), terminal_verification_scope=getattr(error,'terminal_verification_scope','Command result/cleanup unverified'), all_external_descendants_verified_terminal=False, peak_sampled_child_rss_bytes=getattr(error,'peak_sampled_child_rss_bytes',None), rss_scope='Direct command only; descendants unmeasured')
            save(attempt/'receipt.json', request)
        raise


def admitted_json(data, *, event_cap=100000):
    """Preflight structure before decoding a bounded local JSON object."""
    try:text=data.decode('utf-8') if isinstance(data,bytes) else data
    except UnicodeError as error:raise ValueError('Metadata UTF-8 unadmitted') from error
    depth=0;events=0;quoted=False;escaped=False
    for character in text:
        if quoted:
            if escaped:escaped=False
            elif character=='\\':escaped=True
            elif character=='"':quoted=False
        elif character=='"':quoted=True
        elif character in '{[':
            depth+=1;events+=1
            if depth>64:raise ValueError('Metadata nesting bound')
        elif character in '}]':depth-=1
        elif character in ',:':events+=1
        if events>event_cap:raise ValueError('Metadata structural event bound')
    def unique(items):
        out={}
        for key,value in items:
            if key in out:raise ValueError('Duplicate metadata field')
            out[key]=value
        return out
    def finite(value):raise ValueError('Nonfinite metadata')
    def floating(value):
        result=float(value)
        if not math.isfinite(result):raise ValueError('Nonfinite metadata number')
        return result
    try:value=json.loads(text,object_pairs_hook=unique,parse_constant=finite,parse_float=floating)
    except (UnicodeError,RecursionError) as error:raise ValueError('Metadata decoding unadmitted') from error
    if not isinstance(value,dict):raise ValueError('Metadata must be an object')
    return value,events


def factor_json(path, limit=65536):
    """Bound regular metadata reads; refuse links, duplicate keys and drift."""
    path=Path(path)
    for part in (path,*path.parents):
        if part.is_symlink():raise ValueError('Linked factor metadata path')
    fd=os.open(path,os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size>limit:
            raise ValueError('Factor metadata is not bounded regular input')
        with os.fdopen(fd,'rb',closefd=False) as stream:data=stream.read(limit+1)
        after=os.fstat(fd);named=path.lstat()
        def identity(info):return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
        if len(data)>limit or identity(before)!=identity(after) or identity(after)!=identity(named):
            raise ValueError('Factor metadata changed during read')
        value,_=admitted_json(data)
        return value
    finally:os.close(fd)


def factor_digest(path, limit=67108864):
    path=Path(path)
    for part in (path,*path.parents):
        if part.is_symlink():raise ValueError('Linked factor digest path')
    fd=os.open(path,os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size>limit:raise ValueError('Factor digest input unadmitted')
        digest=hashlib.sha256();count=0
        while True:
            chunk=os.read(fd,min(1048576,limit-count+1))
            if not chunk:break
            count+=len(chunk)
            if count>limit:raise ValueError('Factor digest input grew beyond bound')
            digest.update(chunk)
        after=os.fstat(fd);named=path.lstat()
        def identity(info):return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
        if identity(before)!=identity(after) or identity(after)!=identity(named):raise ValueError('Factor digest input changed')
        return digest.hexdigest()
    finally:os.close(fd)


def reference_path(path):
    path=Path(path)
    if not path.is_absolute() or '..' in path.parts:
        raise ValueError('Reference path must be absolute without parent traversal')
    for part in (path,*path.parents):
        try:info=part.lstat()
        except FileNotFoundError:continue
        if stat.S_ISLNK(info.st_mode):raise ValueError('Linked reference path')
        if part!=path and not stat.S_ISDIR(info.st_mode):raise ValueError('Reference parent is not a directory')
    return path


def reference_tree(path):
    path=reference_path(path)
    try:info=path.lstat()
    except FileNotFoundError:return
    if not stat.S_ISDIR(info.st_mode):raise ValueError('Reference root is not a directory')
    pending=[(path,0)];count=0
    while pending:
        current,depth=pending.pop()
        if depth>16:raise ValueError('Reference namespace depth bound')
        with os.scandir(current) as entries:
            for entry in entries:
                count+=1
                if count>100000:raise ValueError('Reference namespace entry bound')
                info=entry.stat(follow_symlinks=False)
                if stat.S_ISDIR(info.st_mode):pending.append((Path(entry.path),depth+1))
                elif not stat.S_ISREG(info.st_mode):raise ValueError('Reference namespace contains linked or special entry')


def reference_copy(source,target,expected):
    source=reference_path(source);target=reference_path(target)
    try:target.lstat()
    except FileNotFoundError:pass
    else:
        if factor_digest(target)!=expected:raise ValueError('Retained reference source changed')
        return
    # Create-only copy with a bounded admitted source read. A failed or interrupted
    # publication is retained and held on the next read, never overwritten.
    fd=os.open(source,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size>67108864:raise ValueError('Reference source byte bound')
        digest=hashlib.sha256();count=0
        with target.open('xb') as out:
            while True:
                chunk=os.read(fd,1048576)
                if not chunk:break
                count+=len(chunk)
                if count>67108864:raise ValueError('Reference source grew beyond bound')
                digest.update(chunk);out.write(chunk)
            out.flush();os.fsync(out.fileno())
        after=os.fstat(fd);named=source.lstat()
        identity=lambda v:(v.st_dev,v.st_ino,v.st_size,v.st_mtime_ns,v.st_ctime_ns)
        if digest.hexdigest()!=expected or identity(before)!=identity(after) or identity(after)!=identity(named):
            raise ValueError('Reference source changed during freezing')
        if factor_digest(target)!=expected:raise ValueError('Frozen reference source changed')
    finally:os.close(fd)


REFERENCE_OWNER_ENV='PHYSICS_SIM_REFERENCE_OWNER'


def reference_owner_paths(root,data):
    root=reference_path(root);data=reference_path(data)
    if not root.is_dir() or not data.is_relative_to(root):raise ValueError('Reference ownership root unadmitted')
    parts=data.relative_to(root).parts
    if len(parts)>=3 and parts[0]=='build' and parts[1].startswith('c3d-'):base=root/'build'
    elif len(parts)>=4 and parts[:2]==('data','experiments'):base=root/'data/experiments'
    else:raise ValueError('Reference ownership overlaps protected storage')
    selected=data.parent
    ancestors=[base/path for path in reversed(selected.relative_to(base).parents)]
    lock=lambda path:root/'tmp/locks'/('build-'+hashlib.sha256(str(path).encode()).hexdigest()+'.lock')
    return selected,[root/'tmp/locks/clean.lock',*[lock(path) for path in ancestors],lock(selected)]


def reference_serial_paths(root,data):
    selected,_=reference_owner_paths(root,data)
    base=Path(root)/('build' if selected.is_relative_to(Path(root)/'build') else 'data/experiments')
    scopes=[base/path for path in reversed(selected.relative_to(base).parents)]+[selected]
    return [Path(root)/'tmp/locks'/('reference-'+hashlib.sha256(str(path).encode()).hexdigest()+'.lock') for path in scopes]


def build_reference_paths(root,owner):
    root=reference_path(root);owner=reference_path(owner);base=root/'build'
    if not owner.is_relative_to(base):raise ValueError('Inherited build scope outside checkout/build')
    scopes=[base/path for path in reversed(owner.relative_to(base).parents)]+[owner]
    return [root/'tmp/locks/clean.lock']+[root/'tmp/locks'/('build-'+hashlib.sha256(str(path).encode()).hexdigest()+'.lock') for path in scopes]


def verify_reference_locks(paths,descriptors,exclusive):
    if not isinstance(descriptors,(list,tuple)) or len(descriptors)!=len(paths) or any(type(fd) is not int or fd<0 for fd in descriptors):
        raise ValueError('Reference inherited ownership descriptors')
    for index,(fd,path) in enumerate(zip(descriptors,paths)):
        reference_path(path)
        witness=os.open(path,os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK)
        try:
            a=os.fstat(fd);b=os.fstat(witness);named=path.lstat()
            if (not stat.S_ISREG(a.st_mode) or a.st_nlink!=1 or
                    (a.st_dev,a.st_ino)!=(b.st_dev,b.st_ino) or (b.st_dev,b.st_ino)!=(named.st_dev,named.st_ino)):
                raise ValueError('Reference inherited lock identity mismatch')
            probe=fcntl.LOCK_SH if index in exclusive else fcntl.LOCK_EX
            try:fcntl.flock(witness,probe|fcntl.LOCK_NB)
            except BlockingIOError:pass
            else:raise ValueError('Reference inherited lock not held')
            mode=fcntl.LOCK_EX if index in exclusive else fcntl.LOCK_SH
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Reference claimed descriptor does not own lock')
        finally:os.close(witness)
    return tuple(descriptors)


def reference_owner_descriptors(directory):
    raw=os.environ.get(REFERENCE_OWNER_ENV)
    if raw is None:return ()
    if len(raw)>8192:raise ValueError('Reference inherited ownership metadata bound')
    value,_=admitted_json(raw.encode())
    if set(value)!={'repo','data','build_root','fds'}:raise ValueError('Reference inherited ownership fields')
    if not isinstance(value['repo'],str) or not isinstance(value['data'],str):raise ValueError('Reference ownership path type')
    selected,paths=reference_owner_paths(value['repo'],value['data'])
    if not reference_path(directory).is_relative_to(selected):raise ValueError('Reference command escapes inherited ownership')
    owner=value['build_root']
    if owner is not None:
        if not isinstance(owner,str) or not selected.is_relative_to(reference_path(owner)):
            raise ValueError('Reference inherited build does not cover scope')
        paths=build_reference_paths(value['repo'],owner)
    serial=reference_serial_paths(value['repo'],value['data'])
    exclusive={len(paths)-1,len(paths)+len(serial)-1}
    return verify_reference_locks(paths+serial,value['fds'],exclusive)


def covering_build_reference(root,selected):
    raw=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
    if raw is None:return None,()
    owner=reference_path(raw)
    if not selected.is_relative_to(owner):return None,()
    paths=build_reference_paths(root,owner)
    encoded=os.environ.get('PHYSICS_SIM_BUILD_HIERARCHY_FDS','')
    if len(encoded)>8192:raise ValueError('Inherited build descriptor metadata bound')
    value,_=admitted_json(('{"fds":'+encoded+'}').encode())
    fds=value['fds']
    verified=verify_reference_locks(paths,fds,{len(paths)-1})
    for key,index in (('PHYSICS_SIM_BUILD_GLOBAL_FD',0),('PHYSICS_SIM_BUILD_ROOT_FD',-1)):
        text=os.environ.get(key,'')
        if not re.fullmatch('[0-9]{1,10}',text) or int(text)!=verified[index]:
            raise ValueError('Inherited build descriptor aliases mismatch')
    return str(owner),verified


@contextmanager
def reference_ownership(root,data):
    if threading.current_thread() is not threading.main_thread():
        raise ValueError('Reference ownership environment requires the main thread')
    selected,paths=reference_owner_paths(root,data)
    reference_tree(data);reference_tree(Path(data).parent/'supervisor-source')
    prior=os.environ.get(REFERENCE_OWNER_ENV)
    if prior is not None:
        borrowed=reference_owner_descriptors(selected)
        yield borrowed
        return
    owner,borrowed=covering_build_reference(root,selected)
    if owner is not None:paths=build_reference_paths(root,owner)
    serial=reference_serial_paths(root,data)
    all_paths=paths+serial;exclusive={len(paths)-1,len(all_paths)-1}
    for path in all_paths:
        reference_path(path)
        try:info=path.lstat()
        except FileNotFoundError:continue
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1:raise ValueError('Reference lock is not an owned regular namespace entry')
    paths[0].parent.mkdir(parents=True,exist_ok=True)
    acquired=[];descriptors=list(borrowed)
    try:
        # Borrowed build descriptors keep their original lock mode. A separate
        # reference hierarchy excludes competing recipes within the same Make.
        begin=len(paths) if borrowed else 0
        for index in range(begin,len(all_paths)):
            path=all_paths[index];reference_path(path)
            fd=os.open(path,os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK,0o600);acquired.append(fd);descriptors.append(fd)
            info=os.fstat(fd);named=path.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or (info.st_dev,info.st_ino)!=(named.st_dev,named.st_ino):
                raise ValueError('Reference lock identity unadmitted')
            mode=fcntl.LOCK_EX if index in exclusive else fcntl.LOCK_SH
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Reference namespace held by cleanup or another writer')
        os.environ[REFERENCE_OWNER_ENV]=json.dumps({'repo':str(root),'data':str(data),'build_root':owner,'fds':descriptors})
        reference_owner_descriptors(selected)
        yield tuple(descriptors)
    finally:
        os.environ.pop(REFERENCE_OWNER_ENV,None)
        for fd in acquired:os.close(fd)


def reference_prepare(root,data,sources,runner):
    root=reference_path(root);data=reference_path(data);runner=reference_path(runner)
    if not root.is_dir():raise ValueError('Reference checkout root missing')
    if not data.is_relative_to(root):raise ValueError('Reference run root escapes checkout')
    relative=data.relative_to(root)
    parts=relative.parts
    allowed=(len(parts)>=3 and parts[0]=='build' and parts[1].startswith('c3d-')) or (len(parts)>=4 and parts[:2]==('data','experiments'))
    if not allowed:raise ValueError('Reference run root overlaps protected storage')
    if not isinstance(sources,tuple) or not 1<=len(sources)<=256 or len(set(sources))!=len(sources):
        raise ValueError('Reference source inventory unadmitted')
    for name in sources:
        if not isinstance(name,str) or Path(name).name!=name or name in ('.','..'):
            raise ValueError('Reference source must be an immediate filename')
    hashes={name:factor_digest(reference_path(root/'scripts'/name)) for name in sources}
    runner_hash=factor_digest(runner)
    digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    directory=data/digest;frozen=directory/'source';supervisor_root=data.parent/'supervisor-source'
    reference_tree(directory);reference_tree(supervisor_root)
    reference_path(frozen)
    # Preflight every retained source before allocating any missing destination.
    for source,target,expected in [(root/'scripts'/name,frozen/name,hashes[name]) for name in sources]+[(runner,supervisor_root/(runner_hash+'.py'),runner_hash)]:
        reference_path(target)
        try:target.lstat()
        except FileNotFoundError:continue
        if factor_digest(target)!=expected:raise ValueError('Retained reference source changed')
    frozen.mkdir(parents=True,exist_ok=True);supervisor_root.mkdir(parents=True,exist_ok=True)
    for name in sources:reference_copy(root/'scripts'/name,frozen/name,hashes[name])
    reference_copy(runner,supervisor_root/(runner_hash+'.py'),runner_hash)
    return hashes,digest,directory,frozen


def reference_artifact_digest(directory,path):
    directory=Path(directory);path=Path(path)
    if not path.is_absolute():path=directory/path
    if '..' in path.parts or not path.is_relative_to(directory) or path==directory:
        raise ValueError('Reference artifact escapes its run')
    return factor_digest(path,8589934592)


def reference_expected_inventory(directory,paths):
    directory=Path(directory)
    expected=set();present=set()
    for raw in paths:
        path=Path(raw)
        if not path.is_absolute():path=directory/path
        if '..' in path.parts or not path.is_relative_to(directory) or path==directory:
            raise ValueError('Expected reference artifact escapes its run')
        name=str(path)
        if name in expected:raise ValueError('Duplicate expected reference artifact')
        expected.add(name)
        # A linked or special leaf is present and must be rejected by the digest,
        # never interpreted as missing and omitted from a failed run inventory.
        try:path.lstat()
        except FileNotFoundError:continue
        present.add(name)
    if not 1<=len(expected)<=10000:raise ValueError('Expected reference inventory bound')
    return expected,present


def reference_artifact_hashes(directory,paths,*,byte_cap=17179869184):
    """Preflight the entire pass, then hash under admitted size/identity bounds."""
    if type(byte_cap) is not int or not 0<=byte_cap<=17179869184:
        raise ValueError('Reference aggregate hash budget unadmitted')
    paths=tuple(paths)
    expected,present=reference_expected_inventory(directory,paths)
    admitted={};total=0
    identity=lambda v:(v.st_dev,v.st_ino,v.st_mode,v.st_size,v.st_mtime_ns,v.st_ctime_ns)
    for name in sorted(present):
        path=reference_path(name);info=path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_size>8589934592:
            raise ValueError('Reference artifact regular-file/byte bound')
        total+=info.st_size
        if total>byte_cap:raise ValueError('Reference aggregate hash byte bound')
        admitted[name]=(info.st_size,identity(info))
    result={}
    for name,(size,witness) in admitted.items():
        # Enforce the preflight size, so growth cannot consume the unused
        # aggregate allowance or the looser per-file cap.
        if identity(Path(name).lstat())!=witness:raise ValueError('Reference hash input changed before read')
        result[name]=factor_digest(name,size)
        if identity(Path(name).lstat())!=witness:raise ValueError('Reference hash input changed during pass')
    if reference_expected_inventory(directory,paths)!=(expected,present):
        raise ValueError('Reference artifact presence changed during pass')
    for name,(_,witness) in admitted.items():
        if identity(Path(name).lstat())!=witness:raise ValueError('Reference hash inventory changed during pass')
    return result


def complete_reference_receipt(value,directory,paths):
    expected,present=reference_expected_inventory(directory,paths)
    if set(value['artifact_sha256'])!=present:
        raise ValueError('Fresh reference artifact inventory incomplete or unexpected')
    if (value['returncode']==0 and value['stop_reason'] is None
            and value.get('diagnostic_failure') is None and expected!=present):
        value['diagnostic_failure']={'kind':'missing_required_artifacts','paths':sorted(expected-present)}


def reference_sync_directory(path):
    path=reference_path(path)
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)


def publish_reference_receipt(path,value):
    """Keep failed candidates; publish complete admitted bytes without replacement."""
    path=reference_path(path)
    if not reference_owner_descriptors(path.parent):raise ValueError('Reference receipt publication requires namespace ownership')
    data=(json.dumps(value,indent=2,allow_nan=False)+'\n').encode('utf-8')
    if len(data)>16777216:raise ValueError('Reference receipt publication byte bound')
    admitted_json(data)
    attempts=reference_path(path.parent/'.receipt-attempts');reference_tree(attempts)
    attempts.mkdir(exist_ok=True)
    attempt=attempts/uuid.uuid4().hex;attempt.mkdir()
    request=attempt/'request.json';candidate=attempt/'receipt.candidate.json'
    digest=hashlib.sha256(data).hexdigest()
    with request.open('x') as stream:
        json.dump({'schema':'physics_sim_reference_receipt_publication_v1','destination':path.name,
                   'sha256':digest,'bytes':len(data),'replacement_allowed':False},stream,indent=2)
        stream.write('\n');stream.flush();os.fsync(stream.fileno())
    with candidate.open('xb') as stream:
        stream.write(data);stream.flush();os.fsync(stream.fileno())
    if factor_digest(candidate,len(data))!=digest:raise ValueError('Reference receipt candidate changed')
    reference_sync_directory(attempt);reference_sync_directory(attempts)
    # link is atomic and create-only: a raced or pre-existing final identity
    # remains untouched. The candidate/request remain even on failed publication.
    reference_path(path);os.link(candidate,path,follow_symlinks=False)
    reference_sync_directory(path.parent)
    if factor_digest(path,len(data))!=digest:raise ValueError('Published reference receipt changed')
    return {'receipt':str(path),'candidate':str(candidate),'sha256':digest,'bytes':len(data)}


def reference_receipt(path,directory,command,hashes,*,expected_paths):
    value=factor_json(path,16777216)
    if value.get('command')!=command or value.get('source_sha256')!=hashes:
        raise ValueError('Reference receipt request/source identity mismatch')
    code=value.get('returncode');reason=value.get('stop_reason')
    if (code is not None and (type(code) is not int or not -128<=code<=255)) or (reason is not None and (not isinstance(reason,str) or not 1<=len(reason)<=4096)) or (code is None and reason is None):
        raise ValueError('Reference receipt outcome unadmitted')
    supervision=value.get('supervision')
    if not isinstance(supervision,dict) or supervision.get('all_external_descendants_verified_terminal') is not False:
        raise ValueError('Reference receipt terminal scope unadmitted')
    artifacts=value.get('artifact_sha256')
    if not isinstance(artifacts,dict) or not 1<=len(artifacts)<=10000:
        raise ValueError('Reference receipt artifact inventory unadmitted')
    expected,present=reference_expected_inventory(directory,expected_paths)
    if set(artifacts)!=present:
        raise ValueError('Reference receipt artifact inventory incomplete or unexpected')
    if code==0 and reason is None and value.get('diagnostic_failure') is None and present!=expected:
        raise ValueError('Reference success missing required artifacts')
    for name,digest in artifacts.items():
        if not isinstance(name,str) or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest):
            raise ValueError('Reference receipt artifact identity unadmitted')
    if reference_artifact_hashes(directory,expected_paths)!=artifacts:
        raise ValueError('Reference receipt artifact bytes changed')
    return value


def reference_progress(path):
    path=Path(path)
    for part in (path,*path.parents):
        if part.is_symlink():raise ValueError('Linked reference progress path')
    fd=os.open(path,os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size>67108864:raise ValueError('Reference progress input bound')
        result=[];count=0;events=0;lines=0
        with os.fdopen(fd,'rb',closefd=False) as stream:
            while True:
                line=stream.readline(65537)
                if not line:break
                count+=len(line);lines+=1
                if len(line)>65536 or count>67108864 or lines>100000:raise ValueError('Reference progress read bound')
                try:
                    item,used=admitted_json(line,event_cap=100000-events)
                except json.JSONDecodeError as error:
                    if line.lstrip().startswith(b'{'):raise ValueError('Malformed reference progress JSON') from error
                    continue
                except ValueError:
                    # Plain diagnostic text is allowed; JSON-shaped invalid
                    # records remain an actionable hold rather than ignored.
                    if line.lstrip().startswith(b'{'):raise
                    continue
                events+=used
                if 'phase' in item or 'iteration' in item:result.append(item)
        after=os.fstat(fd);named=path.lstat()
        def identity(info):return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
        if identity(before)!=identity(after) or identity(after)!=identity(named):raise ValueError('Reference progress changed')
        return result
    finally:os.close(fd)


def factor_binding(directory, relative, digest, command, library_digest):
    if (not isinstance(relative,str) or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest) or
        not isinstance(library_digest,str) or not re.fullmatch('[0-9a-f]{64}',library_digest) or
        not isinstance(command,list) or not command or not all(isinstance(x,str) for x in command)):
        raise ValueError('Factor binding fields unadmitted')
    part=Path(relative)
    if part.is_absolute() or len(part.parts)!=3 or part.parts[0]!='.compiler-attempts' or part.name!='receipt.json' or '..' in part.parts:
        raise ValueError('Factor compiler receipt path unadmitted')
    proof=directory/part
    if factor_digest(proof,65536)!=digest:raise ValueError('Factor compiler receipt digest changed')
    value=factor_json(proof)
    if value.get('status')!='passed' or value.get('requested_command')!=command or value.get('output_sha256')!=library_digest:
        raise ValueError('Factor compiler receipt binding mismatch')
    if not isinstance(value.get('output'),str):raise ValueError('Factor output path unadmitted')
    output=Path(value['output'])
    if output.parent!=Path('.') or output.name in ('','..') or factor_digest(directory/output)!=library_digest:
        raise ValueError('Factor library binding mismatch')
    if factor_digest(proof,65536)!=digest:raise ValueError('Factor compiler receipt changed')
    return value


def publish_factor_record(path, text, compile_result):
    """Retain metadata candidate then publish by atomic no-replace link."""
    path=Path(path);directory=path.parent
    value=json.loads(text)
    relative=compile_result['compile_receipt'];digest=factor_digest(directory/relative,65536)
    factor_binding(directory,relative,digest,value['command'],value['library_sha256'])
    value.update(factor_record_schema='physics_sim_factor_build_v2',compile_receipt=relative,
                 compile_receipt_sha256=digest,all_external_descendants_verified_terminal=False)
    candidate=(directory/relative).parent/(path.name+'.candidate')
    with candidate.open('x') as stream:
        json.dump(value,stream,indent=2,allow_nan=False);stream.write('\n');stream.flush();os.fsync(stream.fileno())
    os.link(candidate,path,follow_symlinks=False)
    return read_factor_record(path)


def read_factor_record(path):
    path=Path(path);value=factor_json(path)
    if value.get('factor_record_schema')!='physics_sim_factor_build_v2':
        raise ValueError('Unbound factor build record held')
    factor_binding(path.parent,value['compile_receipt'],value['compile_receipt_sha256'],value['command'],value['library_sha256'])
    return value


def factor_tool(command,directory,tag,*,cwd):
    """Retain bounded identity-tool output and actual command supervision."""
    execute(command,directory,tag,15,512*1024**2,log_cap=65536,cwd=cwd)
    path=directory/(tag+'.stdout')
    with path.open('rb') as stream:data=stream.read(65537)
    if len(data)>65536:raise ValueError('Factor tool identity output bound')
    return SimpleNamespace(stdout=data.decode('utf-8'))


if __name__=='__main__':
    if len(sys.argv)==7 and sys.argv[1]=='--cfd-anchor':
        anchor_child(int(sys.argv[2]),int(sys.argv[3]),json.loads(sys.argv[4]),json.loads(sys.argv[5]),int(sys.argv[6]))
