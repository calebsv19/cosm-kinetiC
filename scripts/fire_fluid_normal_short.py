"""Bounded normal-space comparison, then scalar-time isolation, to .5 s."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative
from fire_fluid_faint_kernel import arrival


def main():
    p = argparse.ArgumentParser()
    for name in ['base', 'half', 'previous', 'references', 'faint', 'output']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--reuse-native', action='store_true', help='Re-audit existing native outputs without rerunning gas')
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=a.reuse_native)
    identities = {}
    for name, build in [('base', a.base), ('half', a.half)]:
        identity = json.loads((build/'identity.json').read_text())
        assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v
                   for k, v in identity['inputs'].items()), 'source drift'
        assert '-DPHYSICS_SIM_LOCALIZED_COLUMNS=1' in identity['flags']
        assert ('-DPHYSICS_SIM_LOCAL_MICRO_FACTOR=2' in identity['flags']) == (name == 'half')
        identities[name] = identity
        (out/(name+'-identity.json')).write_text(json.dumps(identity, indent=2)+'\n')
    assert identities['base']['inputs'] == identities['half']['inputs']
    inputs = {}

    def read(path):
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return [json.loads(x) for x in path.read_text().splitlines()]

    def run(command, path, cap):
        return read(path) if a.reuse_native else execute(command, path, cap)

    previous = read(a.previous/'case-128-64-5.jsonl')
    old_identity = json.loads((a.previous/'qualified-final/identity.json').read_text())
    # Check unchanged transport/transfer helpers and pinned owner inputs before
    # reusing the prior 64-normal prefix, rather than another full gas run.
    for path, digest in identities['base']['inputs'].items():
        if path in old_identity['inputs'] and Path(path).name not in [
                'build_thermal_transport.py', 'physics_sim_thermal_columns_probe.c',
                'physics_sim_localized_columns.c']:
            assert digest == old_identity['inputs'][path], ('baseline drift', path)
    for name in ['columns-test', 'face-test', 'support-test', 'slab-test']:
        v = subprocess.run([a.base/name], capture_output=True, text=True, timeout=30, check=True)
        (out/(name+'.log')).write_text(v.stdout+v.stderr)
    control = run([a.base/'probe', 128, 64, 5, 80, 10], out/'base64-control.jsonl', 60)
    assert control[:-1] == previous[:3], 'baseline prefix not exact'
    cases = [dict(name='normal64', rows=previous[:21], terminal=previous[-1], reused=True)]
    print('64-normal retained prefix admitted by exact control', flush=True)

    refs = {key: read(a.references/f'reference-{key}.jsonl')[:21]
            for key in ['base', 'modes', 'normal', 'time']}
    gas_tail = read(a.faint/'tail-time.jsonl')
    # One fixed contour qualifies the weighted .5-s history, but not its
    # exponentially smaller first samples. Use their separately qualified probes.
    rare = json.loads((a.faint/'qualified-final/report.json').read_text())
    inputs[str((a.faint/'qualified-final/report.json').resolve())] = hashlib.sha256(
        (a.faint/'qualified-final/report.json').read_bytes()).hexdigest()
    bounds = [arrival(i*.025, 128) for i in range(21)]
    reference_changes = {key: max(relative([r[key] for r in refs[lo]],
                                          [r[key] for r in refs[hi]])
                                  for lo, hi in [('base', 'modes'), ('modes', 'normal'), ('normal', 'time')])
                         for key in ['transport_k', 'near_k']}
    assert all(v < .005 for v in reference_changes.values())

    def analyze(case):
        rows = case['rows']
        assert [r['tick'] for r in rows] == list(range(0, 801, 40))
        assert all(np.isfinite(v).all() for r in rows for v in r.values())
        terminal = case['terminal']
        assert terminal['memory_live_bytes'] == 0 and terminal['velocity_error'] == 0
        assert terminal['refusal_checks'] == 6 and terminal['minimum_k'] >= 0
        assert max(abs(r['gas_balance_j']) for r in rows) < 1e-8
        assert max(abs(r['solid_balance_j']) for r in rows) < 1e-9
        assert abs(rows[-1]['source_j']-.05888671875) < 1e-10
        for r in rows:
            assert abs(sum(r['local_q_j'])-r['q_j']) < 1e-12
            assert abs(50/8*sum(r['solid0_k']+r['solid1_k'])-r['q_j']) < 1e-12
        metrics = {key: relative([r[key] for r in rows], [r[key] for r in refs['time']])
                   for key in ['transport_k', 'near_k']}
        metrics['tail_k'] = relative([r['tail_k'] for r in rows], [r['gas_k'] for r in gas_tail])
        low = np.array([r['lower_q_j'] for r in bounds])
        high = np.array([r['upper_q_j'] for r in bounds])
        actual = np.array([r['q_j'] for r in rows])
        lower = float(np.maximum(np.maximum(low-actual, actual-high), 0).sum()/high.sum())
        upper = float(np.maximum(np.abs(actual-low), np.abs(actual-high)).sum()/low.sum())
        points = {}
        for i in [1, 4, 16, 20]:
            lo, hi = bounds[i]['lower_q_j'], bounds[i]['upper_q_j']
            points[str(i*.025)] = dict(native_q_j=rows[i]['q_j'], physical_q_interval_j=[lo, hi],
                ratio_to_upper_bound=rows[i]['q_j']/hi,
                gas_tail_k=rows[i]['tail_k'], reference_tail_k=(
                    rare['gas_tail_checks']['selected_points'][str(i*.025)]['reference_k']
                    if i in [1, 4] else gas_tail[i]['gas_k']))
        case.update(metrics=metrics, wall_heat_error_interval=[lower, upper],
            gates={**{k: v <= .1 for k, v in metrics.items()},
                   'q_j': True if upper <= .1 else False if lower > .1 else None},
            points=points,
            solid_given_native_q={str(d): relative([r[f'solid{d}_k'] for r in rows],
                                                  [r[f'replay{d}_k'] for r in rows]) for d in [0, 1]})
        # Weak local solids are not re-qualified by this short aggregate check.
        case['all_local_solid_qualified'] = False

    analyze(cases[0])
    for name, build in [('normal128', a.base), ('normal128-half-scalar', a.half)]:
        sanitizer = run([build/'probe-sanitize', 128, 128, 5, 80, 10], out/(name+'-sanitize.jsonl'), 90)
        strict = run([build/'probe', 128, 128, 5, 80, 10], out/(name+'-smoke.jsonl'), 90)
        assert strict == sanitizer, 'strict/sanitizer mismatch'
        result = run([build/'probe', 128, 128, 5, 800, 10], out/(name+'.jsonl'), 600)
        terminal = result.pop()
        assert terminal['steps'] == 800 and len(result) == 21
        assert result[:3] == strict[:-1]
        case = dict(name=name, rows=result, terminal=terminal, reused=False)
        analyze(case)
        cases.append(case)
        summary = {k: v for k, v in case.items() if k != 'rows'}
        print(json.dumps(summary), flush=True)
        (out/'progress.json').write_text(json.dumps(dict(cases=[{k: v for k, v in c.items() if k != 'rows'} for c in cases]), indent=2)+'\n')
    assert cases[0]['terminal']['fine_substeps'] == cases[1]['terminal']['fine_substeps'] == 4
    assert cases[2]['terminal']['fine_substeps'] == 8
    differences = {}
    for name, lo, hi in [('space', 0, 1), ('scalar_time', 1, 2)]:
        differences[name] = {key: relative([r[key] for r in cases[lo]['rows']],
                                           [r[key] for r in cases[hi]['rows']])
                             for key in ['transport_k', 'near_k', 'tail_k', 'q_j']}
    result = dict(threshold=.1, interval_s=[0, .5], native_exchange_window_s=5/1600,
        receiver_dt_s=1/1600, source_end_s=.4, startup_restriction_s=782/1600,
        cases=[{k: v for k, v in c.items() if k != 'rows'} for c in cases],
        differences=differences, reference_changes=reference_changes, inputs=inputs,
        shared_inputs_pinned=True, full_box_qualified=False)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v for k, v in inputs.items())
    (out/'report.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
