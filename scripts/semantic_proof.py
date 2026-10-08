"""Retain each local semantic compiler attempt without resetting prior outputs."""
import argparse
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import uuid

from clean_outputs import no_symlinks
from build_owner import inherited_descriptors, run as owned_run
from cfd_evidence import sha, seal_bundle


def run(repo, build, parent, object_path, log_path, command):
    repo = repo.resolve()
    def local(path):
        path = Path(os.path.abspath(path))
        original = Path(os.path.abspath(Path.cwd()))
        if path.is_relative_to(original) and original.resolve() == repo:
            path = repo/path.relative_to(original)
        no_symlinks(path, repo)
        return path
    build, parent, object_path, log_path = map(local, (build, parent, object_path, log_path))
    if not build.is_relative_to(repo/'build'):
        raise ValueError('Semantic build selection must be inside checkout/build')
    if not parent.is_relative_to(repo/'data/experiments'):
        raise ValueError('Semantic evidence parent must be inside checkout/data/experiments')
    for path in (object_path, log_path):
        if path == build or not path.is_relative_to(build):
            raise ValueError('Semantic output hint must be inside selected build root')
    if object_path == log_path or object_path.suffix != '.o':
        raise ValueError('Semantic object and log selection must be distinct')
    if (command.count('-o') != 1 or command.count('-c') != 1
            or '--dump-sema' not in command):
        raise ValueError('Semantic command requires --dump-sema and one -c/-o')
    out_index, source_index = command.index('-o')+1, command.index('-c')+1
    if max(out_index, source_index) >= len(command):
        raise ValueError('Incomplete semantic compiler command')
    if local(Path(command[out_index])) != object_path:
        raise ValueError('Semantic compiler output differs from declared hint')
    source = local(Path(command[source_index]))
    if not source.is_file():
        raise ValueError('Semantic source must be a regular checkout file')
    source_digest = sha(source)
    compiler = shutil.which(command[0])
    if compiler is None:
        raise ValueError('Semantic compiler is unavailable')
    compiler = Path(compiler).resolve()
    compiler_digest = sha(compiler)
    parent.mkdir(parents=True, exist_ok=True)
    capsule = parent/('semantic-'+uuid.uuid4().hex)
    capsule.mkdir()
    # Preserve the source under inspection and the exact requested command.
    (capsule/'source.c').write_bytes(source.read_bytes())
    if sha(capsule/'source.c') != source_digest:
        raise ValueError('Semantic source changed during capture')
    argv = list(command)
    argv[out_index] = str(capsule/object_path.name)
    proof = {'schema': 'physics_sim_semantic_proof_v1', 'artifact_class': 'semantic_proof',
             'status': 'running', 'source': str(source), 'source_sha256': source_digest,
             'requested_command': command, 'executed_command': argv,
             'legacy_object_hint': str(object_path), 'legacy_log_hint': str(log_path),
             'legacy_outputs_modified': False, 'build_root': str(build),
             'compiler': str(compiler), 'compiler_sha256': compiler_digest,
             'control_sha256': sha(Path(__file__).resolve()),
             'complete_dependency_capture': False,
             'environment': {'FISICS_MAX_PROCS': os.environ.get('FISICS_MAX_PROCS')}}
    (capsule/'request.json').write_text(json.dumps(proof, indent=2)+'\n')
    print('Semantic proof retained: '+str(capsule), file=sys.stderr, flush=True)
    child = None
    signals = []
    previous = {}
    def forward(number, frame):
        signals.append(number)
        if child is not None and child.poll() is None:
            os.killpg(child.pid, number)
    try:
        for number in (signal.SIGTERM, signal.SIGINT):
            previous[number] = signal.signal(number, forward)
        owner = os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
        descriptors = inherited_descriptors(repo, Path(owner)) if owner else ()
        with (capsule/'compiler.log').open('xb') as log:
            child = subprocess.Popen(argv, cwd=repo, stdout=log, stderr=subprocess.STDOUT,
                                     start_new_session=True, pass_fds=descriptors)
            code = child.wait()
            log.flush(); os.fsync(log.fileno())
        proof['exit_code'] = code
        output = capsule/object_path.name
        if signals:
            code = 128+signals[-1]
        elif code == 0 and (not output.is_file() or output.is_symlink()):
            code = 2
            proof['failure'] = 'Compiler returned success without a regular object'
        elif sha(source) != source_digest or sha(compiler) != compiler_digest:
            code = 2
            proof['failure'] = 'Semantic source or compiler changed during compilation'
        proof['status'] = 'passed' if code == 0 else 'failed'
        proof['return_code'] = code
        (capsule/'receipt.json').write_text(json.dumps(proof, indent=2)+'\n')
        seal_bundle(capsule)
        return code if code >= 0 else 128-code
    except (OSError, ValueError) as error:
        proof.update(status='failed', failure=str(error), return_code=2)
        if not (capsule/'receipt.json').exists():
            (capsule/'receipt.json').write_text(json.dumps(proof, indent=2)+'\n')
        # Unexpected compiler-created links are intentionally not sealed as valid evidence.
        if not (capsule/'bundle_manifest.json').exists():
            seal_bundle(capsule)
        return 2
    finally:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM); child.wait()
        for number, handler in previous.items():
            signal.signal(number, handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build-root', 'parent', 'object', 'log'):
        parser.add_argument('--'+name, required=True, type=Path)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    try:
        repo = Path.cwd().resolve()
        selected = Path(os.path.abspath(args.build_root))
        if not inherited_descriptors(repo, selected):
            raise SystemExit(owned_run(selected, [sys.executable, '-B', str(Path(__file__).resolve()), *sys.argv[1:]]))
        raise SystemExit(run(Path.cwd(), args.build_root, args.parent, args.object, args.log, command))
    except (ValueError, OSError) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__': main()
