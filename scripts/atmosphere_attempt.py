"""Retain native atmosphere execution and acceptance diagnostics for local operators."""
from contextlib import contextmanager
from contextvars import ContextVar
from functools import wraps
import hashlib
import json
import os
from pathlib import Path
import stat
import time
import uuid
from cfd_evidence import admitted_path, experiment_root

CURRENT=ContextVar('atmosphere_attempt',default=None)
CONTROLS=('atmosphere_attempt.py','passive_atmosphere.py','evolving_atmosphere.py',
          'open_atmosphere.py','tool_probe.py','build_owner.py','numeric_digest_stream.py',
          'surface_sources/growth_fire_v1.py')

def payload(path,limit):
    path=admitted_path(path)
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size>limit:
            raise ValueError('Atmosphere input is not a bounded regular file: '+str(path))
        with os.fdopen(fd,'rb',closefd=False) as stream:data=stream.read(limit+1)
        after=os.fstat(fd);selected=path.stat()
        identity=lambda row:(row.st_dev,row.st_ino,row.st_mode,row.st_size,row.st_mtime_ns,row.st_ctime_ns)
        if len(data)>limit or identity(before)!=identity(after) or identity(after)!=identity(selected):
            raise ValueError('Atmosphere input changed during capture')
        return data,stat.S_IMODE(after.st_mode)
    finally:os.close(fd)

def exclusive(path,data):
    path=admitted_path(path)
    with path.open('xb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())

def encoded(value,limit=268435456):
    data=json.dumps(value,allow_nan=False,separators=(',',':')).encode()+b'\n'
    if len(data)>limit:raise ValueError('Atmosphere retained JSON exceeds byte bound')
    return data

def flush_directory(path):
    fd=os.open(admitted_path(path),os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)

def receipt(capsule,state):
    pending=capsule/('receipt-'+uuid.uuid4().hex+'.pending')
    exclusive(pending,encoded(state,1048576));os.replace(pending,capsule/'receipt.json')
    flush_directory(capsule)

@contextmanager
def retained_directory():
    current=CURRENT.get()
    if current is None:raise ValueError('Native atmosphere request requires retained attempt')
    yield current[0]

def retain_capture(row,command):
    current=CURRENT.get()
    if current is None:return
    capsule,state=current
    if 'command' in state:raise ValueError('Atmosphere attempt already executed a worker')
    state['command']={k:v for k,v in row.items() if k not in ('stdout','stderr')}
    state['command']['argv']=command
    state['terminal_verification_scope']=row.get('terminal_verification_scope')
    state['direct_capture_anchor_reaped']=row.get('direct_child_reaped') is True
    state['all_external_descendants_verified_terminal']=False
    exclusive(capsule/'stdout.bin',row['stdout']);exclusive(capsule/'stderr.bin',row['stderr'])
    receipt(capsule,state)

def retained_atmosphere(repo,role):
    def decorate(operation):
        @wraps(operation)
        def wrapped(request,worker,*args,**kwargs):
            parent=experiment_root(repo)/'atmosphere-attempts'
            parent=admitted_path(parent);parent.mkdir(parents=True,exist_ok=True)
            capsule=parent/(role+'-'+uuid.uuid4().hex);capsule.mkdir()
            state={'schema':'physics_sim_atmosphere_attempt_v1','artifact_class':'retained_evidence',
                   'status':'running','role':role,'started_at':time.time(),
                   'terminal_processes_verified':False,'complete_dependency_capture':False}
            receipt(capsule,state)
            token=CURRENT.set((capsule,state));frozen={}
            try:
                exclusive(capsule/'input.json',encoded(request))
                worker=admitted_path(worker)
                sources=[(worker,'worker',134217728)]+[(repo/'scripts'/name,'control/'+name,4194304) for name in CONTROLS]
                for path,name,limit in sources:
                    data,mode=payload(path,limit)
                    target=capsule/'source'/name;target.parent.mkdir(parents=True,exist_ok=True)
                    exclusive(target,data);frozen[path]=(target,hashlib.sha256(data).hexdigest(),mode)
                if not frozen[worker][2]&0o111:raise ValueError('Atmosphere worker is not executable')
                for directory in sorted({target.parent for target,_,_ in frozen.values()},key=lambda p:len(p.parts),reverse=True):
                    flush_directory(directory)
                flush_directory(capsule/'source');flush_directory(capsule);flush_directory(parent)
                state['inputs']={str(p):{'sha256':d,'mode':m} for p,(_,d,m) in frozen.items()}
                receipt(capsule,state)
                result=operation(request,worker,*args,**kwargs)
                for path,(target,digest,mode) in frozen.items():
                    data,current_mode=payload(path,134217728)
                    preserved,_=payload(target,134217728)
                    if current_mode!=mode or hashlib.sha256(data).hexdigest()!=digest or hashlib.sha256(preserved).hexdigest()!=digest:
                        raise ValueError('Atmosphere worker/control input changed')
                command=state.get('command',{})
                if command.get('status')!='passed' or command.get('terminal_processes_verified') is not True:
                    raise ValueError('Atmosphere command completion unverified')
                exclusive(capsule/'accepted.json',encoded(result))
                state.update(status='completed',terminal_processes_verified=True)
                return result
            except BaseException as error:
                command=state.get('command')
                terminal=command is None or command.get('terminal_processes_verified') is True
                state.update(status='failed_retained' if terminal else 'held',terminal_processes_verified=terminal,failure=str(error))
                if isinstance(error,ValueError):error.args=(str(error)+'; retained attempt: '+str(capsule),)
                raise
            finally:
                state['finished_at']=time.time()
                try:receipt(capsule,state)
                finally:CURRENT.reset(token)
        return wrapped
    return decorate
