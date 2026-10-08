"""Run a local compiler with staged output; publish only after successful completion."""
import argparse
import json
import math
import re
import select
import shutil
import sys
import time
import os
from pathlib import Path
import signal
import stat
import subprocess
import tempfile

from clean_outputs import ROOT_EXECUTABLES, no_symlinks
from build_owner import inherited, inherited_descriptors
from build_outputs import record, verified, dependency_input_snapshot, linker_dependency_records, link_input_snapshot
from check_clean_root import build_witness



def compiler_anchor(result_fd, lifeline_fd, descriptors, command):
    """Keep a direct live child anchoring the compiler's process group."""
    for number in (signal.SIGINT, signal.SIGTERM):
        signal.signal(number, lambda *_: None)
    try:
        child = subprocess.Popen(command, pass_fds=descriptors)
        while child.poll() is None:
            if select.select([lifeline_fd], [], [], .05)[0] and not os.read(lifeline_fd, 1):
                os.killpg(os.getpgrp(), signal.SIGKILL)
                raise RuntimeError('Compiler anchor self-cleanup failed')
        result = str(child.returncode)
    except OSError as error:
        result = '!' + str(error.errno)
    os.write(result_fd, (result + '\n').encode('ascii'))
    os.close(result_fd)
    while os.read(lifeline_fd, 1):
        pass
    os.killpg(os.getpgrp(), signal.SIGKILL)
    raise RuntimeError('Compiler anchor self-cleanup failed')


def signal_compiler_group(child, number):
    """Signal only while the single waiter's direct anchor remains unreaped."""
    if child.returncode is not None:
        raise ValueError('Compiler anchor was already reaped')
    observed = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
    if observed is None:
        try:
            if os.getpgid(child.pid) != child.pid:
                raise ValueError('Compiler anchor group identity changed')
        except ProcessLookupError:
            if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is None:
                raise
    try:
        os.killpg(child.pid, number)
        return True
    except ProcessLookupError:
        return False


def supervise_compiler(command, descriptors=(), wall_cap=900):
    """Observe compiler result and anchor cleanup before allowing publication.

    This verifies the direct command/anchor only, not escaped descendants.
    Unknown terminal ownership raises, leaving the staging directory held.
    """
    if type(wall_cap) not in (int, float) or not math.isfinite(wall_cap) or not 0 < wall_cap <= 86400:
        raise ValueError('Invalid compiler wall-time bound')
    if not all(hasattr(os, name) for name in ('waitid', 'P_PID', 'WEXITED', 'WNOHANG', 'WNOWAIT')):
        raise ValueError('Compiler supervision requires non-reaping child observation')
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
            raise ValueError('Compiler result pipe closed prematurely')
        result_bytes += chunk
        if len(result_bytes) > 16:
            raise ValueError('Compiler result protocol exceeded bound')
        if b'\n' in result_bytes:
            if re.fullmatch(rb'![0-9]{1,5}\n', result_bytes):
                launch_errno = int(result_bytes[1:-1])
            else:
                if not re.fullmatch(rb'-?[0-9]{1,3}\n', result_bytes):
                    raise ValueError('Malformed compiler result')
                code = int(result_bytes)
                if not -128 <= code <= 255:
                    raise ValueError('Compiler result outside exit-code range')
            observed = True

    def forward(number, frame):
        interrupted.append(number)
        if child is not None:
            try:
                signal_compiler_group(child, number)
            except (OSError, ValueError) as error:
                errors.append(str(error))

    try:
        for number in (signal.SIGINT, signal.SIGTERM):
            previous[number] = signal.signal(number, forward)
        child = subprocess.Popen([sys.executable, '-B', str(Path(__file__).resolve()),
            '--compiler-anchor', str(result_write), str(life_read), json.dumps(list(descriptors)),
            json.dumps(list(command))], start_new_session=True,
            pass_fds=(*descriptors, result_write, life_read))
        os.close(result_write); result_write = None
        os.close(life_read); life_read = None
        deadline = time.monotonic() + wall_cap
        while not observed:
            if interrupted:
                break
            if time.monotonic() >= deadline:
                raise ValueError('Compiler exceeded wall-time bound')
            if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                raise ValueError('Compiler anchor exited before result handoff')
            if select.select([result_read], [], [], .05)[0]:
                read_result()
    finally:
        prior_error = sys.exc_info()[1]
        if child is not None:
            if not observed:
                try:
                    signal_compiler_group(child, signal.SIGTERM)
                except (OSError, ValueError) as error:
                    errors.append(str(error))
                deadline = time.monotonic() + 1
                while not observed and time.monotonic() < deadline:
                    try:
                        if select.select([result_read], [], [], .05)[0]:
                            read_result()
                        if os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT) is not None:
                            break
                    except (OSError, ValueError) as error:
                        errors.append(str(error)); break
            try:
                signal_compiler_group(child, signal.SIGKILL)
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
            raise ValueError('Compiler terminal ownership unverified; staging retained: ' +
                '; '.join(errors or [str(prior_error or 'command result unobserved')]))
    if launch_errno is not None:
        raise OSError(launch_errno, 'Compiler launch failed')
    return 128 + interrupted[-1] if interrupted else code if code >= 0 else 128 - code


def dependency_text(path):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        before=os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_nlink!=1 or before.st_size>1024*1024:
            raise ValueError('Unclassified generated compiler dependency')
        with os.fdopen(fd,'rb',closefd=False) as stream:data=stream.read(before.st_size+1)
        if (len(data)!=before.st_size or build_witness(os.fstat(fd))!=build_witness(before)
                or build_witness(path.lstat())!=build_witness(before)):
            raise ValueError('Generated compiler dependency changed during read')
        return data.decode('utf-8')
    finally:os.close(fd)


def run(command, repo, link_provenance=False):
    repo = repo.resolve()
    if command.count('-o') != 1:
        raise ValueError('Compiler command must declare exactly one -o output')
    index = command.index('-o') + 1
    if index >= len(command):
        raise ValueError('Missing compiler output')
    output = Path(os.path.abspath(command[index]))
    # Normalize macOS temporary-directory aliases, without following checkout symlinks.
    original = Path(os.path.abspath(Path.cwd()))
    if output.is_relative_to(original) and original.resolve() == repo:
        output = repo/output.relative_to(original)
    no_symlinks(output, repo)
    if not output.is_relative_to(repo/'build') and not (output.parent == repo and output.name in ROOT_EXECUTABLES):
        raise ValueError('Undeclared compiler output: '+str(output))
    if output.exists() and not stat.S_ISREG(output.stat().st_mode):
        raise ValueError('Compiler output must be a regular file')
    if any(value.startswith(('-MF','-MT','-MQ')) for value in command):
        raise ValueError('Explicit dependency paths require a dedicated publication contract')
    dependency = '-MMD' in command or '-MD' in command
    if dependency and (command.count('-c')!=1 or output.suffix!='.o'):
        raise ValueError('Dependency publication requires one object compilation')
    if link_provenance:
        if os.uname().sysname != 'Darwin' or dependency or '-c' in command or any(value.endswith(('.c','.cc','.cpp','.m','.mm')) for value in command):
            raise ValueError('Link provenance requires a Darwin object-only link')
        secondary_outputs=('-dependency_info','-map','-object_path_lto','-save-temps','--save-temps','-serialize-diagnostics','-ftime-trace','-fsave-optimization-record')
        if any(option in value for value in command for option in secondary_outputs):
            raise ValueError('Caller-selected secondary link outputs are unsupported')
        if not output.is_relative_to(repo/'build'):
            raise ValueError('Link provenance sidecars require an isolated build output')
    link_manifest = output.with_name(output.name+'.link-inputs.json')
    destinations = ([output.with_suffix('.d')] if dependency else []) + ([link_manifest] if link_provenance else []) + [output]
    predecessors = {}
    for path in destinations:
        no_symlinks(path, repo)
        predecessors[path] = verified(repo, path) if path.exists() else None
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = tempfile.mkdtemp(prefix='.'+output.name+'.staging-', dir=output.parent)
    terminal_verified = False
    retain_attempt = False
    try:
        staged = Path(temporary)/output.name
        argv = list(command)
        argv[index] = str(staged)
        if dependency:
            argv += ['-MF', str(staged.with_suffix('.d')), '-MQ', command[index]]
        # A dylib's default install identity must not refer to the temporary path.
        if '-dynamiclib' in argv and '-install_name' not in argv:
            argv += ['-install_name', str(output)]
        owner_root=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
        owner_fds=()
        if owner_root and inherited(repo,Path(owner_root)):
            owner_fds=inherited_descriptors(repo,Path(owner_root))
        before_inputs=None;before_witnesses=[];input_budget={'entries':0,'hash_bytes':0}
        if dependency:
            before_dep=Path(temporary)/'inputs-before.d'
            preparation=[value for number,value in enumerate(command)
                         if number not in (index-1,index) and value not in ('-c','-MMD','-MD')]
            preparation += ['-M' if '-MD' in command else '-MM','-MF',str(before_dep),'-MQ',command[index]]
            code=supervise_compiler(preparation,owner_fds)
            terminal_verified=True
            if code:return code if code>0 else 128-code
            before_inputs=dependency_input_snapshot(repo,output.with_suffix('.d'),dependency_text(before_dep),input_budget,before_witnesses)
        before_link=None; before_link_witnesses=[]; link_budget={'entries':0,'hash_bytes':0}
        if link_provenance:
            discovery = list(argv); discovery[index] = str(Path(temporary)/'discovery.bin')
            discovery_trace = Path(temporary)/'discovery.dep'
            discovery += ['-Wl,-dependency_info,'+str(discovery_trace)]
            code = supervise_compiler(discovery, owner_fds)
            terminal_verified = True
            if code: return code if code>0 else 128-code
            link_selection = linker_dependency_records(discovery_trace,repo,Path(temporary)/'discovery.bin')
            before_link = link_input_snapshot(link_selection,link_budget,before_link_witnesses)
            argv += ['-Wl,-dependency_info,'+str(Path(temporary)/'actual.dep')]
        terminal_verified=False
        code = supervise_compiler(argv, owner_fds)
        terminal_verified = True
        if code:
            return code if code > 0 else 128-code
        if not staged.is_file() or staged.is_symlink():
            raise ValueError('Compiler succeeded without a regular staged output')
        if dependency and not staged.with_suffix('.d').is_file():
            raise ValueError('Compiler succeeded without staged dependencies')
        input_snapshot=None
        if dependency:
            after_witnesses=[]
            try:
                input_snapshot=dependency_input_snapshot(repo,output.with_suffix('.d'),dependency_text(staged.with_suffix('.d')),input_budget,after_witnesses)
                if input_snapshot!=before_inputs or after_witnesses!=before_witnesses:
                    raise ValueError('Compiler input bytes, identities or dependency closure changed')
            except (ValueError,OSError) as error:
                retain_attempt=True
                hold={'schema':'physics_sim_compiler_input_hold_v1','artifact_class':'retained_compiler_output',
                      'reason':str(error),'output':str(output),'before':before_inputs,'after':input_snapshot}
                with (Path(temporary)/'input-hold.json').open('x') as stream:
                    json.dump(hold,stream,sort_keys=True);stream.write('\n');stream.flush();os.fsync(stream.fileno())
                raise ValueError('Compiler inputs unconfirmed after execution; retained attempt: '+temporary) from error
            input_snapshot['scope']='compiler_boundary_observation'
        staged_link = Path(temporary)/'link-inputs.json'
        if link_provenance:
            after_link=None; after_link_witnesses=[]
            try:
                after_selection = linker_dependency_records(Path(temporary)/'actual.dep',repo,staged)
                after_link = link_input_snapshot(after_selection,link_budget,after_link_witnesses)
                if after_selection != link_selection or after_link != before_link or after_link_witnesses != before_link_witnesses:
                    raise ValueError('Linker input contents, identities or search selection changed')
                link_text=json.dumps({'schema':'physics_sim_link_manifest_v1','selection':after_selection,'snapshot':after_link},sort_keys=True)+'\n'
                if len(link_text.encode())>1024*1024: raise ValueError('Link manifest byte bound exceeded')
                with staged_link.open('x') as stream:
                    stream.write(link_text);stream.flush();os.fsync(stream.fileno())
            except (ValueError,OSError) as error:
                retain_attempt=True
                with (Path(temporary)/'link-input-hold.json').open('x') as stream:
                    json.dump({'schema':'physics_sim_link_input_hold_v1','artifact_class':'retained_compiler_output',
                        'reason':str(error),'output':str(output),'before':before_link,'after':after_link},stream,sort_keys=True)
                    stream.write('\n');stream.flush();os.fsync(stream.fileno())
                raise ValueError('Linker inputs unconfirmed; retained attempt: '+temporary) from error
        # Flush both files before publication. Output is the final commit point.
        for path in ([staged.with_suffix('.d')] if dependency else []) + [staged]:
            with path.open('rb') as stream:
                os.fsync(stream.fileno())
        # Revalidate all predecessors before publishing either output. Unknown
        # or changed files are never silently adopted by a successful build.
        for path, predecessor in predecessors.items():
            no_symlinks(path, repo)
            if predecessor is None:
                if path.exists():
                    raise ValueError('Compiler destination appeared during build: '+str(path))
            elif not path.exists() or verified(repo, path) != predecessor:
                raise ValueError('Compiler predecessor changed during build: '+str(path))
        if dependency:
            os.replace(staged.with_suffix('.d'), output.with_suffix('.d'))
        if link_provenance:
            os.replace(staged_link,link_manifest)
        os.replace(staged, output)
        if dependency:
            dep_record=record(repo, output.with_suffix('.d'), 'dependency',input_snapshot=input_snapshot)
        link_record=record(repo,link_manifest,'linker-inputs') if link_provenance else None
        record(repo, output, 'compiler',dependency_sha256=dep_record['sha256'] if dependency else None,
               link_inputs_sha256=link_record['sha256'] if link_record else None)
        return 0
    finally:
        # Forced death or uncertain cleanup never authorizes stage deletion.
        if terminal_verified and not retain_attempt:
            shutil.rmtree(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--link-provenance',action='store_true')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    try:
        raise SystemExit(run(command, Path.cwd(),args.link_provenance))
    except (ValueError, OSError) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    if len(sys.argv) == 6 and sys.argv[1] == '--compiler-anchor':
        compiler_anchor(int(sys.argv[2]), int(sys.argv[3]), json.loads(sys.argv[4]), json.loads(sys.argv[5]))
    else:
        main()
