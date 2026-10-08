"""Supervise a local integration fixture and retain invocation terminal evidence."""
import argparse
import datetime
import fcntl
import json
import os
import re
from pathlib import Path
import signal
import select
import subprocess
import sys
import time
import uuid

from cfd_evidence import admitted_path, sha
from check_clean_root import read_json
from build_owner import inherited_descriptors


def save(path,row):
    with path.open('x') as stream:
        json.dump(row,stream,indent=2,allow_nan=False);stream.write('\n');stream.flush();os.fsync(stream.fileno())


def context(repo,script=None):
    try:
        directory=admitted_path(Path(os.environ['PHYSICS_SIM_FIXTURE_SESSION_DIR']))
        if directory.parent!=repo/'tmp/fixture-sessions' or len(directory.name)!=32:return None
        fd=int(os.environ['PHYSICS_SIM_FIXTURE_SESSION_FD'])
        identity=os.fstat(fd);lock=directory/'owner.lock';selected=lock.stat()
        if (identity.st_dev,identity.st_ino)!=(selected.st_dev,selected.st_ino):return None
        witness=os.open(lock,os.O_RDONLY|os.O_NOFOLLOW)
        try:
            try:fcntl.flock(witness,fcntl.LOCK_EX|fcntl.LOCK_NB);return None
            except BlockingIOError:pass
        finally:os.close(witness)
        row=read_json(directory/'request.json',16384)
        if not isinstance(row,dict):return None
        if row.get('schema')!='physics_sim_fixture_session_v1' or row.get('session_id')!=directory.name:return None
        if script is not None and row.get('script')!=str(script):return None
        return directory,fd,row
    except (KeyError,OSError,ValueError,TypeError):return None


def anchor_child(result_fd, lifeline_fd, descriptors, script, arguments):
    """Keep a live session leader; loss of the parent's pipe triggers self-cleanup."""
    child = subprocess.Popen(['bash', script, *arguments], pass_fds=descriptors)
    while child.poll() is None:
        if select.select([lifeline_fd], [], [], .05)[0] and not os.read(lifeline_fd, 1):
            os.killpg(os.getpgrp(), signal.SIGKILL)
            raise RuntimeError('Anchor self-cleanup did not terminate its group')
    os.write(result_fd, (str(child.returncode) + '\n').encode('ascii'))
    os.close(result_fd)
    while os.read(lifeline_fd, 1):
        pass
    os.killpg(os.getpgrp(), signal.SIGKILL)
    raise RuntimeError('Anchor self-cleanup did not terminate its group')


def observe_owned_child(child):
    """Observe without reaping; the unreaped child pins its session/group number.

    Requires this supervisor to be the only direct-child waiter. A lost anchor
    holds signaling rather than falling back to a persisted PID/group number.
    """
    if child.returncode is not None:
        raise ValueError('Direct child was already reaped; process-group anchor lost')
    observation = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
    if observation is None:
        try:
            if os.getpgid(child.pid) != child.pid:
                raise ValueError('Direct child no longer identifies its owned process group')
        except ProcessLookupError:
            # macOS does not expose a zombie's group through getpgid. Its
            # waitable, unreaped identity still reserves the numeric PID.
            observation = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
            if observation is None: raise
    return observation


def signal_owned_group(child, number):
    observe_owned_child(child)
    try:
        os.killpg(child.pid, number)
        return True
    except ProcessLookupError:
        return False  # Anchored group already has no signalable members.


def supervise(repo,script,arguments,wall_cap=3600):
    if not all(hasattr(os, name) for name in ('waitid', 'P_PID', 'WEXITED', 'WNOHANG', 'WNOWAIT')):
        raise ValueError('Fixture supervision requires non-reaping child observation')
    repo=repo.resolve();script=admitted_path(script if script.is_absolute() else repo/script)
    if script.parent!=repo/'tests/integration' or not script.name.startswith('run_') or script.suffix!='.sh':
        raise ValueError('Fixture supervisor requires a repository integration script')
    if not 1<=wall_cap<=86400:raise ValueError('Invalid fixture wall cap')
    script_sha=sha(script)
    root=admitted_path(repo/'tmp/fixture-sessions');root.mkdir(parents=True,exist_ok=True)
    directory=root/uuid.uuid4().hex;directory.mkdir()
    fd=os.open(directory/'owner.lock',os.O_CREAT|os.O_EXCL|os.O_RDWR|os.O_NOFOLLOW,0o600)
    # Keep the inherited lifetime descriptor away from shell substitution pipes.
    high=fcntl.fcntl(fd,fcntl.F_DUPFD_CLOEXEC,128);os.close(fd);fd=high
    fcntl.flock(fd,fcntl.LOCK_EX)
    row={'schema':'physics_sim_fixture_session_v1','artifact_class':'fixture_session',
        'session_id':directory.name,'script':str(script),'script_sha256':script_sha,
        'arguments':arguments,'status':'running','wall_cap_s':wall_cap,
        'created_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    save(directory/'request.json',row)
    env=dict(os.environ,PHYSICS_SIM_FIXTURE_SESSION_DIR=str(directory),PHYSICS_SIM_FIXTURE_SESSION_FD=str(fd))
    build=Path(os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT',repo/'build'))
    descriptors=[fd,*inherited_descriptors(repo,build)]
    jobserver=re.search(r'--jobserver-(?:auth|fds)=(\d+),(\d+)',env.get('MAKEFLAGS',''))
    if jobserver:
        for value in jobserver.groups():
            selected=int(value)
            try:os.fstat(selected);descriptors.append(selected)
            except OSError:pass
    result_read,result_write=os.pipe()
    lifeline_read,lifeline_write=os.pipe()
    os.set_blocking(result_read,False)
    child=None;signals=[];handlers={};failure=None;started=time.monotonic();code=2;group_termination_requested=False;direct_child_reaped=False;group_absent_at_cleanup=False;group_cleanup_anchored=False
    def forward(number,frame):
        signals.append(number)
        if child is not None:
            try:signal_owned_group(child,number)
            except (OSError,ValueError):pass
    try:
        for number in (signal.SIGINT,signal.SIGTERM):handlers[number]=signal.signal(number,forward)
        child=subprocess.Popen([sys.executable,'-B',str(Path(__file__).resolve()),'--anchor-child',
            str(result_write),str(lifeline_read),','.join(str(value) for value in descriptors),str(script),*arguments],
            cwd=repo,env=env,pass_fds=[*descriptors,result_write,lifeline_read],start_new_session=True)
        os.close(result_write);result_write=None
        os.close(lifeline_read);lifeline_read=None
        result_bytes=b''
        while True:
            observation = observe_owned_child(child)
            if observation is not None:
                raise ValueError('Fixture anchor exited before result/cleanup handoff')
            try:
                chunk=os.read(result_read,17)
            except BlockingIOError:
                chunk=None
            if chunk is not None:
                if not chunk:raise ValueError('Fixture result pipe closed before result')
                result_bytes+=chunk
                if len(result_bytes)>16:raise ValueError('Fixture result exceeds protocol bound')
                if b'\n' in result_bytes:
                    if not re.fullmatch(rb'-?[0-9]{1,3}\n',result_bytes):
                        raise ValueError('Malformed fixture result')
                    code=int(result_bytes)
                    if not -128<=code<=255:raise ValueError('Fixture result outside exit-code range')
                    break
            if signals:raise ValueError('Fixture interrupted by signal '+str(signals[-1]))
            if time.monotonic()-started>wall_cap:raise ValueError('Fixture exceeded wall cap')
            time.sleep(.05)
        if signals:code=128+signals[-1]
    except (Exception,KeyboardInterrupt) as error:
        failure=str(error);code=128+signals[-1] if signals else 2
    finally:
        if child is not None:
            try:
                group_termination_requested=signal_owned_group(child,signal.SIGKILL)
                group_absent_at_cleanup=not group_termination_requested
                group_cleanup_anchored=True
            except (OSError,ValueError) as error:
                failure='Owned group termination unverified: '+str(error)
            os.close(lifeline_write);lifeline_write=None
            try:
                # Reaping needs a direct-child witness, not a live group lookup.
                # macOS can stop exposing getpgid before exit becomes waitable.
                os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT)
                child.wait(timeout=5)
                direct_child_reaped=True
            except (OSError,ValueError,subprocess.TimeoutExpired) as error:
                failure='Direct child reap unverified: '+str(error)
        if lifeline_write is not None:os.close(lifeline_write)
        if lifeline_read is not None:os.close(lifeline_read)
        os.close(result_read)
        if result_write is not None:os.close(result_write)
        for number,handler in handlers.items():signal.signal(number,handler)
    try:
        if sha(script)!=script_sha:failure='Fixture script changed during invocation';code=2
        if failure is not None and code==0:code=2
        terminal={**row,'status':'passed' if code==0 and failure is None else 'failed','exit_code':code,
            'failure':failure,'wall_s':time.monotonic()-started,'owned_process_group_reaped':False,
            'direct_child_reaped':direct_child_reaped,
            'owned_process_group_termination_requested':group_termination_requested,
            'group_identity_anchored_until_termination':group_cleanup_anchored,
            'owned_process_group_absent_at_cleanup':group_absent_at_cleanup,
            'all_external_descendants_verified_terminal':False,'pruning_authorized':False}
        save(directory/'receipt.json',terminal)
        for registration in directory.glob('capsule-*.json'):
            selected=read_json(registration,16384);capsule=admitted_path(Path(selected['capsule']))
            allowed=(repo/'tmp',repo/'visual_artifacts',repo/'data/experiments')
            if not any(capsule!=root and capsule.is_relative_to(root) for root in allowed):
                raise ValueError('Fixture terminal capsule outside allowed namespaces')
            owner=read_json(capsule/'fixture_owner.json',16384)
            if (not isinstance(owner,dict) or owner.get('capsule_root')!=str(capsule)
                    or owner.get('root')!=str(capsule/'work')
                    or owner.get('artifact_class') not in ('fixture_scratch','fixture_visual_output')):
                raise ValueError('Fixture terminal capsule declaration mismatch')
            if (owner.get('session_id')!=directory.name or owner.get('invocation_id')!=selected['invocation_id']
                    or sha(capsule/'fixture_owner.json')!=selected['owner_sha256']):
                raise ValueError('Fixture capsule owner changed before terminal recording')
            save(capsule/'fixture_terminal.json',{'schema':'physics_sim_fixture_terminal_v1',
                'artifact_class':owner['artifact_class'],'session_id':directory.name,
                'invocation_id':owner['invocation_id'],'owner_sha256':selected['owner_sha256'],
                'session_receipt':str(directory/'receipt.json'),'session_receipt_sha256':sha(directory/'receipt.json'),
                'status':terminal['status'],'exit_code':code,'owned_process_group_reaped':terminal['owned_process_group_reaped'],
                'direct_child_reaped':direct_child_reaped,
                'owned_process_group_termination_requested':group_termination_requested,
                'group_identity_anchored_until_termination':group_cleanup_anchored,
                'owned_process_group_absent_at_cleanup':group_absent_at_cleanup,
                'all_external_descendants_verified_terminal':False,'payload_integrity_verified':False,'pruning_authorized':False})
        return code if code>=0 else 128-code
    finally:os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',required=True,type=Path);parser.add_argument('--script',required=True,type=Path)
    parser.add_argument('--check-session',action='store_true');parser.add_argument('--wall-cap',type=int,default=3600)
    parser.add_argument('arguments',nargs=argparse.REMAINDER);args=parser.parse_args()
    try:
        repo=args.repo.resolve();script=admitted_path(args.script)
        if args.check_session:raise SystemExit(0 if context(repo,script) else 1)
        arguments=args.arguments[1:] if args.arguments[:1]==['--'] else args.arguments
        raise SystemExit(supervise(repo,script,arguments,args.wall_cap))
    except (ValueError,OSError) as error:parser.exit(2,'Fixture session held: '+str(error)+'\n')


if __name__=='__main__':
    if len(sys.argv)>5 and sys.argv[1]=='--anchor-child':
        anchor_child(int(sys.argv[2]),int(sys.argv[3]),[int(value) for value in sys.argv[4].split(',') if value],sys.argv[5],sys.argv[6:])
    else:main()
