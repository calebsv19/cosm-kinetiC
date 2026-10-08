"""Hold local build ownership across an entire Make process tree."""
import argparse
import math
import select
import sys
import time
from contextlib import contextmanager
from contextvars import ContextVar
from functools import wraps
import fcntl
import hashlib
import os
from pathlib import Path
import signal
import subprocess
import re
import json

from clean_outputs import no_symlinks


def lock_path(repo, root):
    return repo/'tmp/locks'/('build-'+hashlib.sha256(str(root).encode()).hexdigest()+'.lock')


def ownership_paths(repo, root):
    """Ancestors are shared intentions; the selected subtree is exclusive."""
    base = repo/'build'
    if not root.is_relative_to(base):
        raise ValueError('Build ownership requires checkout/build')
    ancestors = list(reversed(root.relative_to(base).parents))
    return [repo/'tmp/locks/clean.lock'] + [lock_path(repo, base/path) for path in ancestors] + [lock_path(repo, root)]


def inherited_descriptors(repo, root):
    if os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT') != str(root):
        return ()
    try:
        descriptors = json.loads(os.environ['PHYSICS_SIM_BUILD_HIERARCHY_FDS'])
        paths = ownership_paths(repo, root)
        if not isinstance(descriptors, list) or len(descriptors) != len(paths):
            return ()
        for index,(fd,path) in enumerate(zip(descriptors,paths)):
            if type(fd) is not int:return ()
            no_symlinks(path,repo)
            witness=os.open(path,os.O_RDWR|os.O_NOFOLLOW)
            try:
                a=os.fstat(fd);b=os.fstat(witness)
                if (a.st_dev,a.st_ino)!=(b.st_dev,b.st_ino):return ()
                # Ancestor/shared locks must block an exclusive witness; the
                # selected root/exclusive lock must also block a shared witness.
                mode=fcntl.LOCK_SH if index==len(paths)-1 else fcntl.LOCK_EX
                try:fcntl.flock(witness,mode|fcntl.LOCK_NB)
                except BlockingIOError:pass
                else:return ()
            finally:os.close(witness)
        if int(os.environ['PHYSICS_SIM_BUILD_GLOBAL_FD']) != descriptors[0]:return ()
        if int(os.environ['PHYSICS_SIM_BUILD_ROOT_FD']) != descriptors[-1]:return ()
        # Reaffirm through the claimed open descriptions: another owner's locks
        # must not make unlocked descriptor metadata appear inherited.
        for index in reversed(range(len(descriptors))):
            mode=fcntl.LOCK_EX if index==len(descriptors)-1 else fcntl.LOCK_SH
            try:fcntl.flock(descriptors[index],mode|fcntl.LOCK_NB)
            except BlockingIOError:return ()
        return tuple(descriptors)
    except (KeyError,ValueError,OSError,TypeError):
        return ()


def inherited(repo, root):
    return bool(inherited_descriptors(repo, root))


def acquire(repo, root):
    """Acquire the same cooperative hierarchy for build and execution owners."""
    repo=repo.resolve();root=Path(os.path.abspath(root))
    no_symlinks(root,repo)
    if not root.is_relative_to(repo/'build'):
        raise ValueError('Build ownership requires checkout/build')
    locks=repo/'tmp/locks';no_symlinks(locks,repo);locks.mkdir(parents=True,exist_ok=True)
    descriptors=[]
    try:
        paths=ownership_paths(repo,root)
        for index,path in enumerate(paths):
            mode=fcntl.LOCK_EX if index==len(paths)-1 else fcntl.LOCK_SH
            fd=os.open(path,os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600);descriptors.append(fd)
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Build held by active cleanup or another build: '+str(root))
        return descriptors
    except BaseException:
        for fd in descriptors:os.close(fd)
        raise


_EXECUTION_FDS=ContextVar('physics_worker_execution_fds',default=())


def execution_descriptors():
    return _EXECUTION_FDS.get()


@contextmanager
def worker_execution(repo, worker):
    """Hold a selected worker subtree across hashing, execution and acceptance."""
    repo=repo.resolve();worker=Path(os.path.abspath(worker))
    no_symlinks(worker,repo)
    if not worker.is_relative_to(repo/'build'):
        raise ValueError('Worker execution requires checkout/build; external workers need their owning lifecycle')
    root=worker.parent
    owner=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
    borrowed=tuple(inherited_descriptors(repo,Path(owner))) if owner and worker.is_relative_to(Path(owner)) else ()
    descriptors=borrowed or tuple(acquire(repo,root));token=None
    try:
        no_symlinks(worker,repo)
        if not worker.is_file():raise ValueError('Selected worker is missing: '+str(worker))
        token=_EXECUTION_FDS.set(descriptors)
        yield worker
    finally:
        if token is not None:_EXECUTION_FDS.reset(token)
        if not borrowed:
            for fd in descriptors:os.close(fd)


def owned_worker(repo):
    def decorate(function):
        @wraps(function)
        def wrapped(request,worker,*args,**kwargs):
            with worker_execution(repo,worker) as selected:
                return function(request,selected,*args,**kwargs)
        return wrapped
    return decorate


def build_anchor(result_fd, lifeline_fd, descriptors, command):
    """Keep a direct live child anchoring the build's process group."""
    for number in (signal.SIGINT, signal.SIGTERM):
        signal.signal(number, lambda *_: None)
    try:
        child = subprocess.Popen(command, pass_fds=descriptors)
        while child.poll() is None:
            if select.select([lifeline_fd], [], [], .05)[0] and not os.read(lifeline_fd, 1):
                os.killpg(os.getpgrp(), signal.SIGTERM)
                deadline = time.monotonic() + 5
                while child.poll() is None and time.monotonic() < deadline:
                    time.sleep(.05)
                os.killpg(os.getpgrp(), signal.SIGKILL)
                raise RuntimeError('Build anchor self-cleanup failed')
        result = str(child.returncode)
    except OSError as error:
        result = '!' + str(error.errno)
    os.write(result_fd, (result + '\n').encode('ascii'))
    os.close(result_fd)
    while os.read(lifeline_fd, 1):
        pass
    os.killpg(os.getpgrp(), signal.SIGKILL)
    raise RuntimeError('Build anchor self-cleanup failed')


def signal_build_group(child, number):
    """Signal only while the single waiter's direct anchor remains unreaped."""
    if child.returncode is not None:
        raise ValueError('Build anchor was already reaped')
    observed = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
    if observed is None:
        try:
            if os.getpgid(child.pid) != child.pid:
                raise ValueError('Build anchor group identity changed')
        except ProcessLookupError:
            if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
                raise
    try:
        os.killpg(child.pid, number)
        return True
    except ProcessLookupError:
        return False


def supervise_build(command, descriptors=(), wall_cap=900, env=None):
    """Observe build result and anchor cleanup before allowing publication.

    This verifies the direct command/anchor only, not escaped descendants.
    Unknown terminal ownership raises; cleanup/retirement remain held.
    """
    if type(wall_cap) not in (int, float) or not math.isfinite(wall_cap) or not 0 < wall_cap <= 86400:
        raise ValueError('Invalid build wall-time bound')
    if not all(hasattr(os, name) for name in ('waitid', 'P_PID', 'WEXITED', 'WNOHANG', 'WNOWAIT')):
        raise ValueError('Build supervision requires non-reaping child observation')
    child = None
    result_read, result_write = os.pipe()
    life_read, life_write = os.pipe()
    os.set_blocking(result_read, False)
    previous = {}
    interrupted = []
    errors = []
    result_bytes = b''
    code = None
    launch_errno = None
    observed = False
    anchored = False
    reaped = False

    def read_result():
        nonlocal result_bytes, code, launch_errno, observed
        try:
            chunk = os.read(result_read, 17)
        except BlockingIOError:
            return
        if not chunk:
            raise ValueError('Build result pipe closed prematurely')
        result_bytes += chunk
        if len(result_bytes) > 16:
            raise ValueError('Build result protocol exceeded bound')
        if b'\n' in result_bytes:
            if re.fullmatch(rb'![0-9]{1,5}\n', result_bytes):
                launch_errno = int(result_bytes[1:-1])
            else:
                if not re.fullmatch(rb'-?[0-9]{1,3}\n', result_bytes):
                    raise ValueError('Malformed build result')
                code = int(result_bytes)
                if not -128 <= code <= 255:
                    raise ValueError('Build result outside exit-code range')
            observed = True

    def forward(number, frame):
        interrupted.append(number)
        if child is not None:
            try:
                signal_build_group(child, number)
            except (OSError, ValueError) as error:
                errors.append(str(error))

    try:
        for number in (signal.SIGINT, signal.SIGTERM):
            previous[number] = signal.signal(number, forward)
        child = subprocess.Popen([sys.executable, '-B', str(Path(__file__).resolve()),
            '--build-anchor', str(result_write), str(life_read), json.dumps(list(descriptors)),
            json.dumps(list(command))], env=env, start_new_session=True,
            pass_fds=(*descriptors, result_write, life_read))
        os.close(result_write); result_write = None
        os.close(life_read); life_read = None
        deadline = time.monotonic() + wall_cap
        while not observed:
            if interrupted:
                break
            if time.monotonic() >= deadline:
                raise ValueError('Build exceeded wall-time bound')
            if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                raise ValueError('Build anchor exited before result handoff')
            if select.select([result_read], [], [], .05)[0]:
                read_result()
    finally:
        prior_error = sys.exc_info()[1]
        if child is not None:
            if not observed:
                try:
                    signal_build_group(child, signal.SIGTERM)
                except (OSError, ValueError) as error:
                    errors.append(str(error))
                deadline = time.monotonic() + 5
                while not observed and time.monotonic() < deadline:
                    try:
                        if select.select([result_read], [], [], .05)[0]:
                            read_result()
                        if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                            break
                    except (OSError, ValueError) as error:
                        errors.append(str(error)); break
            try:
                signal_build_group(child, signal.SIGKILL)
                anchored = True
            except (OSError, ValueError) as error:
                errors.append(str(error))
            os.close(life_write); life_write = None
            try:
                child.wait(timeout=5)
                reaped = True
            except (OSError, subprocess.TimeoutExpired) as error:
                errors.append(str(error))
        for fd in (result_read, result_write, life_read, life_write):
            if fd is not None:
                os.close(fd)
        for number, handler in previous.items():
            signal.signal(number, handler)
        if child is not None and (errors or not anchored or not reaped or not observed):
            raise ValueError('Build terminal ownership unverified: ' +
                '; '.join(errors or [str(prior_error or 'command result unobserved')]))
    if launch_errno is not None:
        raise OSError(launch_errno, 'Build launch failed')
    return 128 + interrupted[-1] if interrupted else code if code >= 0 else 128 - code

def run(root, command):
    repo=Path.cwd().resolve();root=Path(os.path.abspath(root))
    descriptors=acquire(repo,root)
    try:
        env=dict(os.environ,PHYSICS_SIM_BUILD_OWNER_ROOT=str(root),
                 PHYSICS_SIM_BUILD_GLOBAL_FD=str(descriptors[0]),PHYSICS_SIM_BUILD_ROOT_FD=str(descriptors[-1]),
                 PHYSICS_SIM_BUILD_HIERARCHY_FDS=json.dumps(descriptors))
        # Preserve GNU Make's already-open jobserver pipe through both hops.
        inherited_fds=list(descriptors)
        match=re.search(r'--jobserver-(?:auth|fds)=(\d+),(\d+)',env.get('MAKEFLAGS',''))
        if match:
            for number in match.groups():
                fd=int(number)
                try:os.fstat(fd)
                except OSError:continue
                inherited_fds.append(fd)
        return supervise_build(command, inherited_fds, env=env)
    finally:
        for fd in descriptors:os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',required=True,type=Path)
    parser.add_argument('--check-inherited',action='store_true')
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    if args.check_inherited:
        print('owned' if inherited(Path.cwd().resolve(),Path(os.path.abspath(args.root))) else 'unowned')
        return
    command=args.command[1:] if args.command[:1]==['--'] else args.command
    if not command:parser.error('Make command is required')
    try:raise SystemExit(run(args.root,command))
    except (ValueError,OSError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':
    if len(sys.argv)==6 and sys.argv[1]=='--build-anchor':
        build_anchor(int(sys.argv[2]),int(sys.argv[3]),json.loads(sys.argv[4]),json.loads(sys.argv[5]))
    else:main()
