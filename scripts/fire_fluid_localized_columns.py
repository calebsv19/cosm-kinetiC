"""Audit one localized startup history against retained independent references."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from fire_fluid_thermal_columns import metrics, local_patch_audit
from fire_fluid_sharp_transport import relative


def main():
    p = argparse.ArgumentParser()
    for name in ['history', 'identity', 'references', 'baseline', 'output']:
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    identity = json.loads(a.identity.read_text())
    assert '-DPHYSICS_SIM_LOCALIZED_COLUMNS=1' in identity['flags']
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v
               for k, v in identity['inputs'].items())
    inputs = {}

    def read(path):
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return [json.loads(x) for x in path.read_text().splitlines()]

    rows = read(a.history)
    terminal = rows.pop()
    assert terminal['terminal'] and terminal['steps'] == 5120
    assert terminal['refusal_checks'] == 6 and terminal['velocity_error'] == 0
    assert terminal['memory_live_bytes'] == 0 and terminal['minimum_k'] >= 0
    assert terminal['lift_balance_max_j'] < 1e-9
    assert terminal['restriction_max_j'] < 1e-12
    assert [r['tick'] for r in rows] == list(range(0, 5121, 40))
    assert all(np.isfinite(v).all() for r in rows for v in r.values())
    gas = max(abs(r['gas_balance_j']) for r in rows)
    solid = max(abs(r['solid_balance_j']) for r in rows)
    assert gas < 1e-8 and solid < 1e-9
    assert abs(rows[-1]['source_j'] - .05888671875) < 1e-10
    for r in rows:
        assert abs(sum(r['local_q_j']) - r['q_j']) < 1e-12
        assert abs(50 / 8 * sum(r['solid0_k'] + r['solid1_k']) - r['q_j']) < 1e-12
    replay = {str(d): relative([r[f'solid{d}_k'] for r in rows],
                              [r[f'replay{d}_k'] for r in rows]) for d in (0, 1)}
    assert all(v <= .1 for v in replay.values())
    refs = {k: read(a.references / f'reference-{k}.jsonl')
            for k in ['base', 'modes', 'normal', 'time']}
    baseline = read(a.baseline)
    if baseline[-1].get('terminal'):
        baseline.pop()
    m = metrics(rows, refs['time'])
    patches = local_patch_audit(rows, refs)
    # Time-prefix values expose startup errors without reference-state resets.
    prefix = {}
    for tick in [40, 160, 640, 800, 2400, 5120]:
        n = tick // 40 + 1
        prefix[str(tick / 1600)] = {}
        for key in ['transport_k', 'near_k', 'tail_k', 'q_j']:
            error = relative([r[key] for r in rows[:n]],
                             [r[key] for r in refs['time'][:n]])
            uncertainty = max(relative([r[key] for r in refs[lo][:n]],
                                       [r[key] for r in refs[hi][:n]])
                              for lo, hi in [('base', 'modes'),
                                             ('modes', 'normal'),
                                             ('normal', 'time')])
            prefix[str(tick / 1600)][key] = dict(
                relative_error=error, reference_relative_change=uncertainty,
                reference_qualified=uncertainty < .005,
                gate_pass=error <= .1 if uncertainty < .005 else None,
                max_reference_signal=max(abs(r[key]) for r in refs['time'][:n]))
    result = dict(threshold=.1, inputs=inputs, native=terminal, metrics=m,
                  baseline_metrics=metrics(baseline, refs['time']),
                  gates={k: v <= .1 for k, v in m.items()},
                  prefix_errors=prefix, local_patch_audit=patches,
                  solid_given_native_q=replay, gas_balance_max_j=gas,
                  solid_balance_max_j=solid,
                  all_reference_qualified_patches_pass=all(
                      v['gate_pass'] is True for values in patches.values()
                      for v in values if v['reference_qualified']),
                  all_local_patches_qualified=all(
                      v['gate_pass'] is True for values in patches.values()
                      for v in values), full_box_qualified=False)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v
               for k, v in inputs.items())
    a.output.mkdir(parents=True, exist_ok=False)
    (a.output / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    (a.output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
