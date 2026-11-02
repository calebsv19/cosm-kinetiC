"""Run one fixed spatial heated-wall range with manifest-bound checkpoint recovery.

Checkpoint is the accepted frontier. Range exports are speculative unless the
receipt says committed; after an uncertain result resume the SAME target tick.
Receiver stages are stateless; replay cannot credit a second external ledger.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(build, sanitized=False):
    manifest = build / 'identity.json'
    data = json.loads(manifest.read_text())
    host = 'probe-sanitize' if sanitized else 'probe'
    worker = 'receiver-worker-sanitize' if sanitized else 'receiver-worker'
    for name in (host, worker):
        if sha(build / name) != data['binaries'][name]:
            raise ValueError('binary differs from manifest: ' + name)
    if '-DPHYSICS_SIM_SHEAR_DURABLE=1' not in data['flags']:
        raise ValueError('not a durable spatial build')
    binding = dict(manifest=sha(manifest), host=data['binaries'][host],
                   worker=data['binaries'][worker], profile='shear-spatial-checkpoint-v1')
    engine = hashlib.sha256(json.dumps(binding, sort_keys=True).encode()).hexdigest()
    return build / host, build / worker, engine, binding


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--checkpoint', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True, help='fresh range export directory')
    p.add_argument('--mode', choices=['new', 'resume'], required=True)
    p.add_argument('--end-tick', type=int, required=True)
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--failure-control', type=int, choices=range(7), default=0)
    a = p.parse_args()
    if not 5 <= a.end_tick <= 3200 or a.end_tick % 5:
        p.error('end tick must be 5..3200, multiple of 5; fixed 6400 Hz clock')
    build, cp, out = a.build.resolve(), a.checkpoint.resolve(), a.output.resolve()
    host, worker, engine, binding = identity(build, a.sanitize)
    if a.mode == 'new' and cp.exists():
        p.error('new checkpoint already exists; inspect/resume same identity')
    if a.mode == 'resume' and not cp.is_file():
        p.error('resume requires existing checkpoint')
    out.mkdir(parents=True, exist_ok=False)
    command = [str(host), '128', '256', '5', '3200', '10', str(out/'packets'),
               str(worker), a.mode, str(cp), str(a.end_tick), engine,
               str(a.failure_control)]
    intent = dict(profile='shear-spatial-checkpoint-v1', checkpoint=str(cp),
                  end_tick=a.end_tick, model_end_s=a.end_tick/6400,
                  binding=binding, engine_sha256=engine, command=command,
                  checkpoint_before=sha(cp) if cp.exists() else None)
    (out/'intent.json').write_text(json.dumps(intent, indent=2)+'\n')
    start = time.monotonic()
    with (out/'history.jsonl').open('xb') as stdout, (out/'stderr.log').open('xb') as stderr:
        result = subprocess.run(command, stdout=stdout, stderr=stderr)
        for f in (stdout, stderr):
            f.flush()
            os.fsync(f.fileno())
    log = (out/'stderr.log').read_text()
    accepted = result.returncode == 0 and ('durable: committed range ' in log or
                                          'durable: reused committed tick ' in log)
    receipt = dict(**intent, returncode=result.returncode, committed=accepted,
                   reused='durable: reused committed tick ' in log,
                   elapsed_wall_s=time.monotonic()-start,
                   checkpoint_after=sha(cp) if cp.exists() else None,
                   exports={f: sha(out/f) for f in ('history.jsonl', 'packets', 'stderr.log')
                            if (out/f).exists()})
    temporary = out/'receipt.next'
    with temporary.open('x') as f:
        json.dump(receipt, f, indent=2)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())
    temporary.rename(out/'receipt.json')
    print(json.dumps({k: receipt[k] for k in ('committed','reused','returncode','elapsed_wall_s')}))
    return 0 if accepted else 1


if __name__ == '__main__':
    raise SystemExit(main())
