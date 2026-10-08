"""Fresh retained worker attempts; failed and uncertain attempts are never removed."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import time
import uuid
from session_paths import checked,directory
from sample_retention import regular,flush_directory
from owned_command import IncompleteTeardown


CONTROL_DIRECTORY=Path(__file__).resolve().parent
MAX_CONTROL_FILES=64
MAX_CONTROL_BYTES=33554432


def capture_controls(directory=CONTROL_DIRECTORY):
    directory=checked(directory);sources={};total=0;entries_seen=0
    with os.scandir(directory) as entries:
        for entry in entries:
            entries_seen+=1
            if entries_seen>256:raise ValueError('Session control directory bound held')
            if not entry.name.endswith('.py'):continue
            if len(sources)>=MAX_CONTROL_FILES:raise ValueError('Session control file bound held')
            data=regular(directory/entry.name,4194304);total+=len(data)
            if total>MAX_CONTROL_BYTES:raise ValueError('Session control byte bound held')
            sources[entry.name]=data
    return sources


# Capture the on-disk control set when this process imports the attempt module.
# Import remains non-mutating; admission failures become retained attempt errors.
try:
    CONTROL_BASELINE=capture_controls();CONTROL_BASELINE_ERROR=None
except (OSError,ValueError) as error:
    CONTROL_BASELINE=None;CONTROL_BASELINE_ERROR=str(error)


def preserve(path,data):
    with path.open('xb') as stream:
        stream.write(data);stream.flush();os.fsync(stream.fileno())
    if regular(path,max(len(data),65536))!=data:raise ValueError('Attempt input readback mismatch')


def verify_preserved(source,controls,worker_bytes):
    expected={'runtime_modules','worker'};observed=set()
    with os.scandir(source) as entries:
        for entry in entries:
            if entry.name not in expected:raise ValueError('Unknown retained attempt source content')
            observed.add(entry.name)
    if observed!=expected:raise ValueError('Retained attempt source content missing')
    modules=checked(source/'runtime_modules');observed=set()
    with os.scandir(modules) as entries:
        for entry in entries:
            if entry.name not in controls:raise ValueError('Unknown retained runtime control content')
            observed.add(entry.name)
    if observed!=set(controls):raise ValueError('Retained runtime control missing')
    for name,data in controls.items():
        if regular(modules/name,4194304)!=data:raise ValueError('Retained runtime control changed')
    if regular(source/'worker',134217728)!=worker_bytes:raise ValueError('Retained worker payload changed')


def record(path,value):
    checked(path)
    temporary=path.with_name('.'+path.name+'.'+uuid.uuid4().hex)
    with temporary.open('xb') as stream:
        stream.write((json.dumps(value,sort_keys=True,allow_nan=False)+'\n').encode())
        stream.flush();os.fsync(stream.fileno())
    checked(path);os.replace(temporary,path);flush_directory(path.parent)


@contextmanager
def attempt(root,phase,worker,request,*,storage_guard=None):
    if phase not in ('author','validate'):raise ValueError('Unknown session attempt phase')
    selected=directory(root/'attempts');selected.mkdir(exist_ok=True)
    capsule=selected/(phase+'-'+uuid.uuid4().hex);capsule.mkdir()
    stage=capsule/'stage';stage.mkdir()
    state={'schema':'physics_sim_session_attempt_v1','artifact_class':'operational_job',
           'phase':phase,'status':'running','started_at':time.time(),'worker_path':str(worker),
           'worker_sha256':None,'worker_bytes':None,'all_external_descendants_verified_terminal':False,'terminal_verification_scope':'Owned command outcome/anchor and local operations only; complete descendants unverified','terminal_processes_verified':False,
           'storage_guard_applied':storage_guard is not None}
    record(capsule/'request.json',dict(state,request=request))
    record(capsule/'receipt.json',state)
    try:
        if storage_guard is not None:storage_guard()
        if CONTROL_BASELINE_ERROR:raise ValueError('Session control baseline held: '+CONTROL_BASELINE_ERROR)
        controls=capture_controls()
        if controls!=CONTROL_BASELINE:raise ValueError('Session control changed since import; restart the source process')
        if not {'attempts.py','owned_command.py','service.py','session_paths.py','sample_retention.py'}<=set(controls):
            raise ValueError('Session control set incomplete')
        source=capsule/'source';source.mkdir()
        modules=source/'runtime_modules';modules.mkdir()
        for name,data in controls.items():preserve(modules/name,data)
        state['control_sha256']={name:hashlib.sha256(data).hexdigest() for name,data in controls.items()}
        state['complete_dependency_capture']=False
        worker_bytes=regular(checked(worker),134217728)
        worker_mode=checked(worker).stat().st_mode & 0o777
        if not worker_mode & 0o111:raise ValueError('Session worker is not executable')
        preserve(source/'worker',worker_bytes)
        flush_directory(modules);flush_directory(source);flush_directory(capsule)
        digest=hashlib.sha256(worker_bytes).hexdigest()
        state.update(worker_sha256=digest,worker_bytes=len(worker_bytes),worker_mode=worker_mode)
        record(capsule/'request.json',dict(state,request=request))
        record(capsule/'receipt.json',state)
        yield capsule,stage,state
        if storage_guard is not None:storage_guard()
        verify_preserved(source,controls,worker_bytes)
        if capture_controls()!=controls:raise ValueError('Session control changed during attempt')
        if checked(worker).stat().st_mode & 0o777 != worker_mode:
            raise ValueError('Session worker permissions changed')
        if hashlib.sha256(regular(checked(worker),134217728)).hexdigest()!=digest:
            raise ValueError('Session attempt worker changed')
        command=state.get('command')
        if (not isinstance(command,dict) or type(command.get('exit_code')) is not int
                or command['exit_code']!=0 or not isinstance(command.get('command'),list)
                or not command['command'] or command['command'][0]!=str(worker)):
            raise IncompleteTeardown('Worker command completion not verified')
        state.update(status='completed',terminal_processes_verified=True,finished_at=time.time())
        record(capsule/'receipt.json',state)
    except BaseException as error:
        # A changed namespace must not receive a failure record intended for the
        # original root. Its original non-terminal receipt stays held.
        if storage_guard is not None:storage_guard()
        state.update(status='held' if isinstance(error,IncompleteTeardown) else 'failed_retained',
                     terminal_processes_verified=not isinstance(error,IncompleteTeardown),
                     failure=str(error),finished_at=time.time())
        record(capsule/'receipt.json',state)
        if isinstance(error,ValueError):error.args=(str(error)+'; retained attempt: '+str(capsule),)
        raise
