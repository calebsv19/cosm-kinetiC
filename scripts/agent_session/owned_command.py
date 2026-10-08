"""Bounded retained command supervision with a live unreaped cleanup anchor."""
import json
import math
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import sys
import time
import threading

class ExecutionFailure(ValueError):
    """Command failed with an observed exit status."""
    def __init__(self, tag, exit_code):
        super().__init__(tag+' exited '+str(exit_code));self.exit_code=exit_code

class IncompleteTeardown(ValueError):
    """Owned cleanup/result unverified; retain an unsealed attempt."""


def anchor_child(result_fd, lifeline_fd, descriptors, command):
    # Caught dispositions reset at command exec; the anchor itself stays alive
    # while the command/nested supervisor receives group termination signals.
    for number in (signal.SIGINT, signal.SIGTERM):signal.signal(number,lambda *_:None)
    try:
        child=subprocess.Popen(command,pass_fds=descriptors)
        while child.poll() is None:
            if select.select([lifeline_fd],[],[],.05)[0] and not os.read(lifeline_fd,1):
                os.killpg(os.getpgrp(),signal.SIGTERM)
                deadline=time.monotonic()+5
                while child.poll() is None and time.monotonic()<deadline:time.sleep(.05)
                os.killpg(os.getpgrp(),signal.SIGKILL)
                raise RuntimeError('Anchor self-cleanup did not terminate group')
        result=str(child.returncode)
    except OSError as error:result='!'+str(error.errno)
    os.write(result_fd,(result+'\n').encode('ascii'));os.close(result_fd)
    while os.read(lifeline_fd,1):pass
    os.killpg(os.getpgrp(),signal.SIGKILL)
    raise RuntimeError('Anchor self-cleanup did not terminate group')


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


def execute(command,cwd,directory,tag,descriptors,wall_cap=900,log_cap=67108864):
    if (type(wall_cap) not in (int,float) or not math.isfinite(wall_cap) or not 0<wall_cap<=86400 or
        type(log_cap) is not int or not 1<=log_cap<=1073741824 or
        not isinstance(tag,str) or not re.fullmatch('[a-zA-Z0-9][a-zA-Z0-9_-]{0,95}',tag)):
        raise ValueError('Invalid retained command tag/time/log bounds')
    if not all(hasattr(os,name) for name in ('waitid','P_PID','WEXITED','WNOHANG','WNOWAIT')):
        raise ValueError('Retained command requires non-reaping child observation')
    command=[os.fsdecode(value) for value in command]
    started=time.monotonic();child=None;signals=[];handlers={};teardown_errors=[]
    result_read,result_write=os.pipe();life_read,life_write=os.pipe();os.set_blocking(result_read,False)
    result_bytes=b'';observed=False;exit_code=None;launch_errno=None;row={};anchored=False;reaped=False
    def read_result():
        nonlocal result_bytes,observed,exit_code,launch_errno
        try:chunk=os.read(result_read,17)
        except BlockingIOError:return
        if not chunk:raise ValueError('Retained command result pipe closed prematurely')
        result_bytes+=chunk
        if len(result_bytes)>16:raise ValueError('Retained command result protocol bound')
        if b'\n' in result_bytes:
            if re.fullmatch(rb'![0-9]{1,5}\n',result_bytes):launch_errno=int(result_bytes[1:-1]);observed=True;return
            if not re.fullmatch(rb'-?[0-9]{1,3}\n',result_bytes):raise ValueError('Malformed retained command result')
            exit_code=int(result_bytes)
            if not -128<=exit_code<=255:raise ValueError('Retained command result out of range')
            observed=True
    def forward(number,frame):
        signals.append(number)
        if child is not None:
            try:signal_owned_group(child,number)
            except (OSError,ValueError) as error:teardown_errors.append(str(error))
    try:
        if threading.current_thread() is threading.main_thread():
            for number in (signal.SIGINT,signal.SIGTERM):handlers[number]=signal.signal(number,forward)
        with (directory/(tag+'.stdout')).open('xb') as out,(directory/(tag+'.stderr')).open('xb') as err:
            child=subprocess.Popen([sys.executable,'-B',str(Path(__file__).resolve()),'--command-anchor',
                str(result_write),str(life_read),json.dumps(list(descriptors)),json.dumps(command)],
                cwd=cwd,stdout=out,stderr=err,start_new_session=True,pass_fds=(*descriptors,result_write,life_read))
            os.close(result_write);result_write=None;os.close(life_read);life_read=None
            while not observed:
                if signals:raise ValueError('Contract interrupted by signal '+str(signals[-1]))
                if time.monotonic()-started>wall_cap:raise ValueError('Contract exceeded wall cap')
                if sum(os.fstat(stream.fileno()).st_size for stream in (out,err))>log_cap:
                    raise ValueError('Contract exceeded retained log cap')
                if os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                    raise ValueError('Retained command anchor exited before handoff')
                if select.select([result_read],[],[],.05)[0]:read_result()
            if signals:raise ValueError('Contract interrupted by signal '+str(signals[-1]))
            if sum(os.fstat(stream.fileno()).st_size for stream in (out,err))>log_cap:
                raise ValueError('Contract exceeded retained log cap')
            if launch_errno is not None:raise OSError(launch_errno,'Retained command launch failed')
            if exit_code:raise ExecutionFailure(tag,exit_code)
            for stream in (out,err):stream.flush();os.fsync(stream.fileno())
            row={'command':command,'exit_code':0,'wall_s':time.monotonic()-started}
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
        scope='Observed command outcome and reaped direct anchor; complete descendant termination unverified'
        row.update(direct_child_reaped=reaped,group_cleanup_identity_anchored=anchored,
                   terminal_verification_scope=scope,all_external_descendants_verified_terminal=False)
        if prior_error is not None:
            prior_error.terminal_verification_scope=scope
            prior_error.all_external_descendants_verified_terminal=False
        if teardown_errors:raise IncompleteTeardown('Contract teardown unverified: '+str(prior_error or '')+'; '+'; '.join(teardown_errors))


if __name__=='__main__' and len(sys.argv)==6 and sys.argv[1]=='--command-anchor':
    anchor_child(int(sys.argv[2]),int(sys.argv[3]),json.loads(sys.argv[4]),json.loads(sys.argv[5]))
