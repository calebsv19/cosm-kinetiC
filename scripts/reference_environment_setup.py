"""Create or reuse an exact retained reference environment; never upgrade in place."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import uuid

from cfd_evidence import admitted_path, sha
from physics_doctor import layout, pins, reference_environment, probe


def write_state(path, value):
    temporary=path.parent/('.state-'+uuid.uuid4().hex)
    try:
        with temporary.open('x') as stream:
            json.dump(value,stream,indent=2,allow_nan=False);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temporary,path)
    finally:temporary.unlink(missing_ok=True)


def execute(command, log_path, descriptors, wall_cap):
    """Retain mutation logs, bound the process group and keep ownership in children."""
    child=None;previous={};signals=[];started=time.monotonic()
    def forward(number,frame):
        signals.append(number)
        if child is not None:
            try:os.killpg(child.pid,number)
            except ProcessLookupError:pass
    try:
        for number in (signal.SIGTERM,signal.SIGINT):previous[number]=signal.signal(number,forward)
        with log_path.open('xb') as log:
            child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,start_new_session=True,
                pass_fds=descriptors,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
            while child.poll() is None:
                if signals:raise ValueError('Setup interrupted by signal '+str(signals[-1]))
                if time.monotonic()-started>wall_cap:raise ValueError('Setup command exceeded wall cap')
                time.sleep(.05)
            if signals:raise ValueError('Setup interrupted by signal '+str(signals[-1]))
            if child.returncode:raise ValueError('Setup command exited '+str(child.returncode))
            log.flush();os.fsync(log.fileno())
        return {'command':command,'exit_code':0,'wall_s':time.monotonic()-started}
    finally:
        if child is not None:
            # Reap the group on failure, even if its immediate parent exited.
            try:os.killpg(child.pid,signal.SIGKILL)
            except ProcessLookupError:pass
            child.wait()
        for number,handler in previous.items():signal.signal(number,handler)


def setup(repo, tools, interpreter, amg=False, apply=False, wall_cap=900, run_command=execute, inspect=reference_environment):
    repo=repo.resolve()
    tools=admitted_path(tools if tools.is_absolute() else repo/tools)
    interpreter=interpreter if interpreter.is_absolute() else repo/interpreter
    interpreter=admitted_path(interpreter.parent)/interpreter.name
    roots=layout(repo,{'build':Path(os.environ.get('PHYSICS_SIM_BUILD_ROOT',repo/'build')),
        'test':Path(os.environ.get('PHYSICS_SIM_TEST_ROOT',repo/'tmp/tests')),
        'experiments':Path(os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT',repo/'data/experiments')),'tools':tools})
    tools=roots['tools'];target=tools/'cfd-reference-venv'
    if interpreter != target/'bin/python':
        raise ValueError('Setup interpreter must be exactly the selected tools-root venv, never a global Python')
    admitted_path(target)
    expected=pins(repo,amg)
    selected_files=('requirements-cfd-reference.txt','requirements-cfd-reference-amg.txt') if amg else ('requirements-cfd-reference.txt',)
    requirements={name:sha(repo/'scripts'/name) for name in selected_files}
    plan={'schema':'physics_sim_reference_setup_plan_v1','target':str(target),'profile':'amg' if amg else 'base',
        'expected':expected,'requirements_sha256':requirements,'existing_environment_modified':False}
    if target.exists():
        state=inspect(repo,interpreter,probe,amg=amg)
        if state['status']=='ready_for_reference_attempt':return {**plan,'status':'reused','readback':state,'mutations_performed':[]}
        return {**plan,'status':'held','readback':state,
            'action':'Preserve this environment; select a fresh REFERENCE_TOOLS_DIR for the requested profile'}
    if not apply:return {**plan,'status':'creation_planned','network_install_required':True}
    if not 1<=wall_cap<=3600:raise ValueError('Setup wall cap must be 1..3600 seconds')
    lock_root=admitted_path(repo/'tmp/locks');lock_root.mkdir(parents=True,exist_ok=True)
    key=hashlib.sha256(str(target).encode()).hexdigest()
    descriptors=[]
    try:
        for path,mode in ((lock_root/'clean.lock',fcntl.LOCK_SH),(lock_root/('reference-env-'+key+'.lock'),fcntl.LOCK_EX)):
            fd=os.open(path,os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600);descriptors.append(fd)
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Reference setup held by active cleanup or another setup')
        # Recheck after acquiring ownership; competing creators must not be adopted.
        admitted_path(target)
        if target.exists():raise ValueError('Reference setup destination appeared; no existing environment modified')
        attempts=admitted_path(tools/'.reference-environment-attempts');attempts.mkdir(parents=True,exist_ok=True)
        attempt=attempts/uuid.uuid4().hex;attempt.mkdir()
        frozen=attempt/'requirements';frozen.mkdir()
        for name,digest in requirements.items():
            (frozen/name).write_bytes((repo/'scripts'/name).read_bytes())
            if sha(frozen/name)!=digest:raise ValueError('Reference requirements changed during capture')
        state={**plan,'schema':'physics_sim_reference_setup_state_v1','artifact_class':'reference_environment','status':'running','attempt':str(attempt),
            'base_python':str(Path(sys.executable).resolve()),'base_python_sha256':sha(Path(sys.executable).resolve()),
            'commands':[],'native_or_numerical_qualification_verified':False}
        write_state(attempt/'request.json',state)
        target.mkdir()  # Exclusive fresh prefix; venvs are not relocated after creation.
        write_state(target/'.reference-setup.json',state)
        print('Reference setup attempt retained: '+str(attempt),file=sys.stderr,flush=True)
        try:
            state['commands'].append(run_command([sys.executable,'-I','-B','-m','venv',str(target)],attempt/'venv.log',tuple(descriptors),min(120,wall_cap)))
            command=[str(interpreter),'-I','-B','-m','pip','--isolated','--disable-pip-version-check','install','--no-input','--no-cache-dir']
            for name in selected_files:command+=['-r',str(frozen/name)]
            state['commands'].append(run_command(command,attempt/'pip.log',tuple(descriptors),wall_cap))
            readback=inspect(repo,interpreter,probe,amg=amg,inspect_setup=False)
            if readback['status']!='ready_for_reference_attempt':raise ValueError('Installed reference profile failed exact pin/import readback')
            if any(sha(repo/'scripts'/name)!=digest for name,digest in requirements.items()):raise ValueError('Reference requirements changed during setup')
            state.update(status='passed',readback=readback)
        except (Exception,KeyboardInterrupt) as error:
            state.update(status='failed',failure=str(error))
        write_state(attempt/'receipt.json',state)
        write_state(target/'.reference-setup.json',state)
        return {**plan,'status':'created' if state['status']=='passed' else 'failed','attempt':str(attempt),
                'receipt':str(attempt/'receipt.json'),'failure':state.get('failure'),'existing_environment_modified':False}
    finally:
        for fd in descriptors:os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-root',required=True,type=Path);parser.add_argument('--interpreter',required=True,type=Path)
    parser.add_argument('--amg',action='store_true');parser.add_argument('--apply',action='store_true')
    parser.add_argument('--wall-cap',type=int,default=900)
    args=parser.parse_args()
    try:row=setup(Path.cwd(),args.tools_root,args.interpreter,args.amg,args.apply,args.wall_cap)
    except (OSError,ValueError,RecursionError) as error:parser.exit(2,'Reference setup held: '+str(error)+'\n')
    print(json.dumps(row,indent=2));raise SystemExit(0 if row['status'] in ('reused','created','creation_planned') else 2)


if __name__=='__main__':main()
