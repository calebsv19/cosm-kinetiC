"""Exterior-only activity capture; ordinary pinned capture ownership/teardown retained."""
import os
import math
import selectors
import signal
import subprocess
import time


import json
import re
import select
import sys
from pathlib import Path


def capture_anchor(result_fd, lifeline_fd, descriptors, command):
    try:
        child = subprocess.Popen(command, pass_fds=descriptors)
        while child.poll() is None:
            if select.select([lifeline_fd], [], [], .05)[0] and not os.read(lifeline_fd, 1):
                os.killpg(os.getpgrp(), signal.SIGKILL)
                raise RuntimeError('Anchor group self-cleanup did not terminate')
        result = str(child.returncode)
    except OSError as error:
        result = '!' + str(error.errno)
    os.write(result_fd, (result + '\n').encode('ascii'))
    os.close(result_fd)
    os.close(1); os.close(2)
    while os.read(lifeline_fd, 1):
        pass
    os.killpg(os.getpgrp(), signal.SIGKILL)
    raise RuntimeError('Anchor group self-cleanup did not terminate')


def signal_capture_group(child, number):
    if child.returncode is not None:
        raise ValueError('Capture group anchor was already reaped')
    observed = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
    if observed is None:
        try:
            if os.getpgid(child.pid) != child.pid:
                raise ValueError('Capture anchor changed process group')
        except ProcessLookupError:
            if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None: raise
    try:
        os.killpg(child.pid, number)
        return True
    except ProcessLookupError:
        return False


def group_activity(group):
    # Ignore the Python cleanup anchor's polling CPU: only native command/children count.
    values=[]
    for path in Path('/proc').glob('[0-9]*/stat'):
        try:
            pid=int(path.parent.name);v=path.read_text().rsplit(')',1)[1].split()
            if pid!=group and int(v[2])==group:
                try:io=(path.parent/'io').read_text()
                except PermissionError:io='unavailable'
                values.append((pid,int(v[19]),int(v[11])+int(v[12]),io))
        except (FileNotFoundError,ProcessLookupError,PermissionError):pass
    return tuple(sorted(values))


def capture(command, env=None, timeout=120, stdout_limit=67108864, stderr_limit=65536, *, combined_limit=None, pass_fds=()):
    """Bound output/time while a live unreaped child anchors cleanup identity."""
    if (type(timeout) not in (int, float) or not math.isfinite(timeout) or not 0 < timeout <= 86400
            or type(stdout_limit) is not int or not 1 <= stdout_limit <= 268435456
            or type(stderr_limit) is not int or not 1 <= stderr_limit <= 1048576
            or (combined_limit is not None and (type(combined_limit) is not int or not 1 <= combined_limit <= 268435456))):
        raise ValueError('Invalid capture wall-time or output bound')
    if not all(hasattr(os, name) for name in ('waitid', 'P_PID', 'WEXITED', 'WNOHANG', 'WNOWAIT')):
        raise ValueError('Capture requires non-reaping child observation')
    limits={'stdout':stdout_limit,'stderr':stderr_limit}
    child=None; interrupted=[]; handlers={}; row={}; teardown_errors=[]
    anchored=False; direct_child_reaped=False; command_result_observed=False
    result_read,result_write=os.pipe(); life_read,life_write=os.pipe()
    def forward(number,frame):
        interrupted.append(number)
        if child is not None:
            try:signal_capture_group(child,number)
            except (OSError,ValueError) as error:teardown_errors.append(str(error))
    selector=selectors.DefaultSelector()
    chunks={'stdout':bytearray(),'stderr':bytearray()}; result_bytes=b''
    started=time.monotonic();last_active=started;next_sample=started;previous_activity=None;code=None
    try:
        for number in (signal.SIGINT,signal.SIGTERM):handlers[number]=signal.signal(number,forward)
        child=subprocess.Popen([sys.executable,'-B',str(Path(__file__).resolve()),'--capture-anchor',
            str(result_write),str(life_read),json.dumps(list(pass_fds)),json.dumps(list(command))],
            env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True,
            pass_fds=(*pass_fds,result_write,life_read))
        os.close(result_write);result_write=None
        os.close(life_read);life_read=None
        for name in chunks:
            stream=getattr(child,name);os.set_blocking(stream.fileno(),False)
            selector.register(stream,selectors.EVENT_READ,name)
        os.set_blocking(result_read,False);selector.register(result_read,selectors.EVENT_READ,'result')
        while selector.get_map():
            if interrupted:raise ValueError('Capture interrupted by signal '+str(interrupted[-1]))
            now=time.monotonic()
            if now>=next_sample:
                sample=(group_activity(child.pid),sum(map(len,chunks.values())))
                if sample!=previous_activity:last_active=now;previous_activity=sample
                next_sample=now+min(1,timeout/4)
            if now-last_active>=timeout:raise ValueError('Native group inactive for %.1fs'%timeout)
            if os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                raise ValueError('Capture anchor exited before result/cleanup handoff')
            for key,_ in selector.select(min(.1,timeout)):
                fd=key.fileobj if isinstance(key.fileobj,int) else key.fileobj.fileno()
                data=os.read(fd,17 if key.data=='result' else 8192)
                if key.data=='result':
                    if not data:raise ValueError('Capture result pipe closed before result')
                    result_bytes+=data
                    if len(result_bytes)>16:raise ValueError('Capture result protocol limit')
                    if b'\n' in result_bytes:
                        if re.fullmatch(rb'![0-9]{1,5}\n',result_bytes):
                            raise ValueError('Capture launch failed errno '+result_bytes[1:-1].decode('ascii'))
                        if not re.fullmatch(rb'-?[0-9]{1,3}\n',result_bytes):raise ValueError('Malformed capture result')
                        code=int(result_bytes)
                        if not -128<=code<=255:raise ValueError('Capture result outside exit-code range')
                        command_result_observed=True;selector.unregister(key.fileobj)
                    continue
                if not data:selector.unregister(key.fileobj);continue
                remaining=limits[key.data]-len(chunks[key.data])
                if combined_limit is not None:remaining=min(remaining,combined_limit-sum(map(len,chunks.values())))
                chunks[key.data].extend(data[:remaining])
                if len(data)>remaining:raise ValueError('Capture exceeded output limit')
        if interrupted:raise ValueError('Capture interrupted by signal '+str(interrupted[-1]))
        row={'status':'passed' if code==0 else 'failed','exit_code':code,
             **{key:bytes(value) for key,value in chunks.items()}}
        return row
    except (OSError,ValueError,subprocess.TimeoutExpired) as error:
        row={'status':'unverified','reason':str(error),**{key:bytes(value) for key,value in chunks.items()}}
        return row
    finally:
        if child is not None:
            try:
                signal_capture_group(child,signal.SIGKILL);anchored=True
            except (OSError,ValueError) as error:teardown_errors.append(str(error))
            os.close(life_write);life_write=None
            try:
                os.waitid(os.P_PID,child.pid,os.WEXITED | os.WNOHANG | os.WNOWAIT)
                child.wait(timeout=5);direct_child_reaped=True
            except (OSError,subprocess.TimeoutExpired) as error:teardown_errors.append(str(error))
            for name in chunks:getattr(child,name).close()
        for fd in (result_read,result_write,life_read,life_write):
            if fd is not None:os.close(fd)
        row.update(aggregate_timeout_s=None,inactivity_window_s=timeout,elapsed_s=time.monotonic()-started,
                   direct_child_reaped=direct_child_reaped,group_cleanup_identity_anchored=anchored,
                   command_result_observed=command_result_observed,
                   terminal_processes_verified=direct_child_reaped and anchored and command_result_observed and not teardown_errors,
                   terminal_verification_scope='Observed command result and reaped direct anchor; complete descendant termination unverified',
                   all_external_descendants_verified_terminal=False)
        if teardown_errors:row.update(status='unverified',reason='Owned process teardown unverified: '+'; '.join(teardown_errors))
        selector.close()
        for number,handler in handlers.items():signal.signal(number,handler)



def probe(command, env=None, timeout=15, limit=65536):
    """Preserve the small combined-output contract for tool identity probes."""
    if (type(timeout) not in (int,float) or not math.isfinite(timeout) or not 0 < timeout <= 60
            or type(limit) is not int or not 1 <= limit <= 1048576):
        raise ValueError('Invalid probe wall-time or output bound')
    row=capture(command,env,timeout,limit,limit,combined_limit=limit)
    for key in ('stdout','stderr'):row[key]=row[key].decode('utf-8',errors='replace').strip()
    return row


def run_worker(worker,request,limit,descriptors=()):
    command=[str(worker),'--field-packet',str(request)]
    row=capture(command,timeout=600,stdout_limit=limit,pass_fds=descriptors)
    from atmosphere_attempt import retain_capture
    retain_capture(row,command)
    if row['status']!='passed':raise ValueError(row.get('reason') or row['stderr'].decode(errors='replace') or 'native failed')
    return row['stdout']


if __name__ == '__main__':
    if len(sys.argv)==6 and sys.argv[1]=='--capture-anchor':
        capture_anchor(int(sys.argv[2]),int(sys.argv[3]),json.loads(sys.argv[4]),json.loads(sys.argv[5]))
