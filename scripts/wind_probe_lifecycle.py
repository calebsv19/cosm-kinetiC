"""Fresh retained Wind probe attempts using existing cooperative worker ownership."""
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import time
import uuid

from build_owner import worker_execution, execution_descriptors
from cfd_evidence import admitted_path
from agent_session.sample_retention import regular, retain, flush_directory
from agent_session.owned_command import IncompleteTeardown


def record(path, value):
    data = (json.dumps(value, sort_keys=True, allow_nan=False) + '\n').encode()
    if len(data) > 1048576:
        raise ValueError('Wind probe record exceeds byte bound')
    retain(path, data)
    flush_directory(path.parent)


def output_parent(repo, value):
    repo = admitted_path(repo)
    parent = admitted_path(value)
    allowed = (repo / 'tmp', repo / 'data/experiments', repo / 'visual_artifacts')
    if not any(parent == root or parent.is_relative_to(root) for root in allowed):
        raise ValueError('Probe output parent requires checkout tmp, data/experiments or visual_artifacts')
    for protected in (repo / 'tmp/locks', repo / 'tmp/fixture-sessions'):
        if parent == protected or parent.is_relative_to(protected):
            raise ValueError('Probe output refuses lifecycle control namespaces')
    for ancestor in (parent, *parent.parents):
        if ancestor == repo:
            break
        marker = ancestor / 'bundle_manifest.json'
        if marker.exists() or marker.is_symlink():
            raise ValueError('Probe output refuses sealed evidence namespaces')
    if parent.exists() and not parent.is_dir():
        raise ValueError('Probe output parent must be a directory')
    return parent


@contextmanager
def attempt(repo, args, source_bytes):
    parent = output_parent(repo, args.output_root)
    worker = admitted_path(args.headless_bin)
    # Execution ownership protects the selected build subtree from cooperative
    # rebuild/clean while its identity is captured and the whole probe runs.
    with worker_execution(repo, worker):
        worker_bytes = regular(worker, 134217728)
        if not worker.stat().st_mode & 0o111:
            raise ValueError('Probe worker is not executable')
        controls = [repo / 'tools/wind_orientation_probe.py',
                    repo / 'scripts/wind_probe_lifecycle.py',
                    repo / 'scripts/agent_session/owned_command.py']
        captured = {p: regular(p, 4194304) for p in controls}
        parent.mkdir(parents=True, exist_ok=True)
        capsule = parent / ('orientation-' + uuid.uuid4().hex)
        capsule.mkdir()
        fd = os.open(capsule / 'owner.lock', os.O_CREAT | os.O_EXCL | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        fcntl.flock(fd, fcntl.LOCK_EX)
        state = {'schema': 'physics_sim_wind_probe_attempt_v1',
                 'artifact_class': 'retained_evidence', 'status': 'running',
                 'attempt_root': str(capsule), 'requested_output_parent': str(parent),
                 'runtime_scene': str(args.runtime_scene), 'worker': str(worker),
                 'worker_sha256': hashlib.sha256(worker_bytes).hexdigest(),
                 'worker_mode': worker.stat().st_mode & 0o777,
                 'control_sha256': {str(p.relative_to(repo)): hashlib.sha256(data).hexdigest() for p, data in captured.items()},
                 'scene_sha256': hashlib.sha256(source_bytes).hexdigest(),
                 'frames': args.frames, 'grid': args.grid, 'orientations': args.orientation,
                 'wall_cap_s': args.wall_cap, 'log_cap_per_case_bytes': args.log_cap,
                 'legacy_keep_existing': args.keep_existing,
                 'parent_reset_performed': False, 'automatic_removal': False,
                 'all_external_descendants_verified_terminal': False,
                 'complete_dependency_capture': False, 'pruning_authorized': False,
                 'commands': []}
        try:
            record(capsule / 'request.json', state)
            retain(capsule / 'source_runtime.json', source_bytes)
            retain(capsule / 'worker', worker_bytes)
            for path, data in captured.items():
                target = capsule / 'control' / path.relative_to(repo)
                target.parent.mkdir(parents=True, exist_ok=True)
                retain(target, data)
            (capsule / 'runtime_scenes').mkdir()
            (capsule / 'logs').mkdir()
            (capsule / 'work').mkdir()
            print('Wind probe retained: ' + str(capsule), flush=True)
            yield capsule, state, (*execution_descriptors(), fd)
            if regular(args.runtime_scene, 2097152) != source_bytes:
                raise ValueError('Probe source changed during execution')
            if regular(worker, 134217728) != worker_bytes:
                raise ValueError('Probe worker changed during execution')
            if any(regular(path, 4194304) != data for path, data in captured.items()):
                raise ValueError('Probe control changed during execution')
            state['status'] = 'passed'
        except BaseException as error:
            state.update(status='held' if isinstance(error, IncompleteTeardown) else 'failed',
                         failure=str(error)[:4096])
            raise
        finally:
            try:
                state['finished_at_unix'] = time.time()
                state['terminal_verification_scope'] = 'Local operation and observed owned command/anchor outcomes only; descendants unverified'
                record(capsule / 'receipt.json', state)
            finally:
                os.close(fd)
