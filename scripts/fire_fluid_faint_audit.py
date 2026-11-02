"""Separate reference space/time/transform errors; never change the 10% gate."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import numpy as np


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--previous', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    inputs = {}

    def read(path):
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return [json.loads(x) for x in path.read_text().splitlines()]

    def case(name):
        return read(a.evidence / (name + '.jsonl'))

    def err(x, y, key):
        v = np.array([r[key] for r in x])
        w = np.array([r[key] for r in y])
        assert v.shape == w.shape
        return float(np.abs(v-w).sum()/np.abs(w).sum())

    patches = {}
    for patch, base, fine, finest, wrap in [
            (5, 'patch5-tilt350', 'p5-space', 'p5-time', 'patch5-wrap'),
            (6, 'p6-contour', 'p6-space', 'p6-time', 'p6-wrap-space')]:
        x, y, z, image = map(case, [base, fine, finest, wrap])
        pairs = [(base, fine), (fine, finest)]
        if patch == 5:
            pairs += [(base, 'p5-cutoff'), (base, 'p5-period'),
                      (base, 'patch5-smoke')]
        else:
            pairs += [(base, 'patch6-tilt700'), (base, 'p6-period'),
                      ('patch6-wrap', 'p6-wrap-space')]
        checks = {lo+' -> '+hi: {key: err(case(lo), case(hi), key)
                                 for key in ['solid0_k', 'solid1_k', 'q_j']}
                  for lo, hi in pairs}
        combined = [{key: r[key]+w[key] for key in ['solid0_k', 'solid1_k', 'q_j']}
                    for r, w in zip(z, image)]
        # Other original-period source images are bounded by the probability
        # that x diffusion reaches their support with maximal rightward drift.
        omitted = 0.
        for j in [-4, -3, -2, 1, 2, 3, 4]:
            lo, hi = patch/32+j*.25, (patch+1)/32+j*.25
            gap = lo-1/16-.05*3.2 if lo > 1/16 else 1/32-hi
            omitted += .05888671875*math.erfc(max(0, gap)/math.sqrt(4*(.026/1206)*3.2))
        native = read(a.previous / 'case-128-64-5.jsonl')[:-1]
        old = read(a.previous.parent / 'fire-fluid-thermal-columns-20261017/runs/reference-time.jsonl')
        per_quantity = {}
        same_normal = case(f'p{patch}-native-normal-image0')
        same_wrap = case(f'p{patch}-native-normal-image-1')
        for key, oldkey in [('solid0_k', 'solid0_k'), ('solid1_k', 'solid1_k'), ('q_j', 'local_q_j')]:
            expected = np.array([r[key] for r in combined])
            actual = np.array([r[oldkey][patch] for r in native])
            prior = np.array([r[oldkey][patch] for r in old])
            matched = np.array([r[key]+w[key] for r, w in zip(same_normal, same_wrap)])
            uncertainty = max(c[key] for c in checks.values())
            error = float(np.abs(actual-expected).sum()/np.abs(expected).sum())
            # Condition estimate is observable-specific; a bad gas sum must
            # not be conflated with the independently well-conditioned solid.
            condition = sum(r['absolute_solid_sum'] for r in z) / sum(
                abs(r['solid0_k'])+abs(r['solid1_k']) for r in z)
            qualified = uncertainty < .005 and condition*np.finfo(float).eps < 1e-8
            per_quantity[key] = dict(reference_change=uncertainty,
                reference_qualified=bool(qualified), native_error=error,
                native_gate_pass=bool(error <= .1) if qualified else None,
                old_reference_error=float(np.abs(prior-expected).sum()/np.abs(expected).sum()),
                native_vs_same_normal=float(np.abs(actual-matched).sum()/np.abs(matched).sum()),
                same_normal_vs_refined=float(np.abs(matched-expected).sum()/np.abs(expected).sum()),
                final_reference=float(expected[-1]), final_native=float(actual[-1]),
                inverse_transform_condition=condition)
        patches[str(patch)] = dict(checks=checks, quantities=per_quantity,
            omitted_image_heat_bound_j=omitted,
            solid_given_native_q={str(d): float(sum(
                abs(r[f'solid{d}_k'][patch]-r[f'replay{d}_k'][patch]) for r in native) /
                sum(abs(r[f'replay{d}_k'][patch]) for r in native)) for d in [0, 1]})

    means = {}
    from fire_fluid_faint_kernel import arrival
    mean_space, mean_time = case('mean-n4096'), case('mean-time')
    native = read(a.previous/'case-128-64-5.jsonl')[:-1]
    old = read(a.previous.parent/'fire-fluid-thermal-columns-20261017/runs/reference-time.jsonl')
    for index in [1, 4, 16, 20]:
        t = index*.025
        k, k2 = arrival(t, 128), arrival(t, 256)
        lo, hi = k2['lower_q_j'], k2['upper_q_j']
        value = native[index]['q_j']
        error_lower = (lo-value)/lo if value < lo else (value-hi)/hi if value > hi else 0
        means[str(t)] = dict(**k2, quadrature_change=abs(k['cold_q_j']/k2['cold_q_j']-1),
            reference_interval_relative_width=(hi-lo)/lo,
            native_q_j=value, old_reference_q_j=old[index]['q_j'],
            native_relative_error_lower_bound=error_lower,
            native_gate_pass=bool(error_lower <= .1),
            fine_cn_q_j=mean_time[index]['q_j'],
            cn_time_change=abs(mean_space[index]['q_j']/mean_time[index]['q_j']-1))
    intervals = [arrival(i*.025, 128) for i in range(21)]
    low = np.array([r['lower_q_j'] for r in intervals])
    high = np.array([r['upper_q_j'] for r in intervals])
    actual = np.array([r['q_j'] for r in native[:21]])
    minimum_distance = np.maximum(np.maximum(low-actual, actual-high), 0)
    mean_history = dict(
        error_lower_bound=float(minimum_distance.sum()/high.sum()),
        error_upper_bound=float(np.maximum(np.abs(actual-low), np.abs(actual-high)).sum()/low.sum()))

    tail_pairs = [('tail-bounded', 'tail-contour'), ('tail-bounded', 'tail-space'),
                  ('tail-space', 'tail-time'), ('tail-bounded', 'tail-period')]
    tails = dict(prefix_half_second={lo+' -> '+hi: err(case(lo), case(hi), 'gas_k')
                                    for lo, hi in tail_pairs})
    for prefix, pairs in [('tail100', [('base', 'contour'), ('base', 'space'), ('base', 'time')]),
                          ('tail25-balanced', [('base', 'cutoff'), ('base', 'space'), ('base', 'time'), ('cutoff', 'contour-cutoff')])]:
        tails[prefix] = {lo+' -> '+hi: err(case(prefix+'-'+lo), case(prefix+'-'+hi), 'gas_k')
                         for lo, hi in pairs}
    half = case('tail-time')
    tails['native_half_second_error'] = float(sum(
        abs(r['tail_k']-s['gas_k']) for r, s in zip(native[:21], half)) /
        sum(abs(s['gas_k']) for s in half))
    tails['half_second_reference_qualified'] = max(tails['prefix_half_second'].values()) < .005
    tails['native_half_second_gate_pass'] = tails['native_half_second_error'] <= .1
    tails['selected_points'] = {
        str(t): dict(reference_k=case(name)[-1]['gas_k'], native_k=native[i]['tail_k'],
                     relative_error=abs(native[i]['tail_k']/case(name)[-1]['gas_k']-1))
        for t, i, name in [(.025, 1, 'tail25-balanced-selected'),
                            (.1, 4, 'tail100-space'), (.5, 20, 'tail-time')]}
    tails['point_check_qualification'] = {
        k: max(tails[k].values()) < .005 for k in ['tail100', 'tail25-balanced']}
    source = Path(__file__).resolve().parents[1]
    for name in ['src/tools/physics_sim_faint_reference.c', 'scripts/fire_fluid_faint_kernel.py',
                 'scripts/fire_fluid_faint_audit.py', 'tests/test_faint_reference_kernel.py',
                 'src/tools/physics_sim_faint_solid_probe.c']:
        path = source/name
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    result = dict(gate=.1, reference_gate=.005, patches=patches,
                  short_mean_wall_heat=means, short_wall_heat_history_error=mean_history,
                  gas_tail_checks=tails, inputs=inputs,
                  native_solver_changed=False, native_rerun=False)
    a.output.mkdir(parents=True, exist_ok=False)
    (a.output/'report.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
