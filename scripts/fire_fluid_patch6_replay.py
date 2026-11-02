"""Exact accepted-packet capture, corrected native replay, independent BE audit."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time
import numpy as np
REUSE = False


def run(command, path, cap, source=None):
    if REUSE:
        receipt = json.loads(path.with_suffix('.receipt.json').read_text())
        assert receipt['returncode'] == 0 and receipt['command'] == list(map(str, command))
        assert receipt['stdin'] == (str(source) if source else None)
        return [json.loads(x) for x in path.read_text().splitlines()]
    start = time.monotonic()
    with path.open('x') as out, path.with_suffix('.log').open('x') as err:
        stream = source.open() if source else None
        try:
            p = subprocess.run(list(map(str, command)), stdin=stream, stdout=out,
                               stderr=err, timeout=cap)
        finally:
            if stream:
                stream.close()
    path.with_suffix('.receipt.json').write_text(json.dumps(dict(
        command=list(map(str, command)), stdin=str(source) if source else None,
        wall_s=time.monotonic()-start, cap_s=cap, returncode=p.returncode))+'\n')
    assert p.returncode == 0, path
    return [json.loads(x) for x in path.read_text().splitlines()]


def main():
    global REUSE
    p = argparse.ArgumentParser()
    for name in ['old', 'corrected', 'previous', 'faint', 'output']:
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--reuse-native', action='store_true')
    a = p.parse_args()
    REUSE = a.reuse_native
    out = a.output.resolve();out.mkdir(parents=True, exist_ok=REUSE)
    inputs = {}

    def read(path):
        inputs[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        return [json.loads(x) for x in path.read_text().splitlines()]

    for name, build, version in [('old', a.old, '0.3.2'), ('corrected', a.corrected, '0.3.3')]:
        identity = json.loads((build/'identity.json').read_text())
        assert identity['core_version'] == version
        assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v
                   for k, v in identity['inputs'].items()), 'source drift'
        assert all(hashlib.sha256((build/k).read_bytes()).hexdigest() == v
                   for k, v in identity['binaries'].items()), 'binary drift'
        (out/(name+'-identity.json')).write_text(json.dumps(identity, indent=2)+'\n')
    prior = read(a.previous/'case-128-64-5.jsonl')
    for executable in ['probe', 'probe-sanitize']:
        trace = out/(executable+'-smoke.packets')
        rows = run([a.old/executable, 128, 64, 5, 80, 10, trace],
                   out/(executable+'-smoke.jsonl'), 90)
        assert rows[:-1] == prior[:3]
    assert (out/'probe-smoke.jsonl').read_bytes() == (out/'probe-sanitize-smoke.jsonl').read_bytes()
    assert (out/'probe-smoke.packets').read_bytes() == (out/'probe-sanitize-smoke.packets').read_bytes()
    print('capture smoke: exact prior/native/sanitizer parity', flush=True)
    trace = out/'patch6.packets'
    captured = run([a.old/'probe', 128, 64, 5, 5120, 10, trace], out/'capture.jsonl', 600)
    assert captured == prior, 'instrumentation or source changed historical trajectory'
    print('full capture: exact historical trajectory; no new gas qualification', flush=True)
    for name in ['columns-test', 'face-test', 'support-test', 'slab-test']:
        p = subprocess.run([a.corrected/name], capture_output=True, text=True, timeout=30, check=True)
        (out/(name+'.log')).write_text(p.stdout+p.stderr)
    native = {}
    for name, executable in [('old', a.old/'patch6-replay'),
                              ('corrected', a.corrected/'patch6-replay'),
                              ('sanitizer', a.corrected/'patch6-replay-sanitize')]:
        native[name] = run([executable], out/(name+'-replay.jsonl'), 90, trace)
        assert native[name][-1] == dict(terminal=True, steps=5120, packets=16384,
                                      refusal_checks=6, timebase_domain=716, ticks_per_second=1600)
        native[name].pop()
    assert native['corrected'] == native['sanitizer']
    text = trace.read_text().splitlines();header = text.pop(0).split()
    assert header[:7] == ['patch6_packets_v1', '128', '64', '5', '5120', '716', '1600']
    assert len(text) == len(native['corrected']) == 1024
    inputs[str(trace)] = hashlib.sha256(trace.read_bytes()).hexdigest()
    packets = []
    for i, line in enumerate(text):
        fields = line.split();assert len(fields) == 3+16*8
        assert list(map(int, fields[:3])) == [5*i, 5*i+5, 5*i+1]
        v = [float.fromhex(x) for x in fields[3:]]
        assert all(math.isfinite(x) for x in v)
        packets.append([v[8*j:8*j+8] for j in range(16)])
    with localcontext() as ctx:
        ctx.prec = 80
        D = Decimal.from_float
        capacity = Decimal(50)/128
        rate = Decimal(2)*Decimal(2)/128/capacity/1600
        energy = [[Decimal(0), Decimal(0)] for _ in range(16)]
        oracle = []
        for row in packets:
            for j, record in enumerate(row):
                for amount in record[3:]:
                    total = energy[j][0]+energy[j][1]+D(amount)
                    difference = (energy[j][0]-energy[j][1]+D(amount))/(1+rate)
                    energy[j] = [(total+difference)/2, (total-difference)/2]
            oracle.append([list(pair) for pair in energy])
        distribution = {}
        for name in ['old', 'corrected']:
            local = []
            for j in range(16):
                item = dict(strip=96+j)
                for d in range(2):
                    reference = [r[j][d] for r in oracle]
                    actual = [D(r['energy_j'][j][d]) for r in native[name]]
                    den = sum(abs(v) for v in reference)
                    if den:
                        error = sum(abs(x-y) for x, y in zip(actual, reference))/den
                        zero_loss = sum(1 for x, y in zip(actual, reference) if not x and y)
                        point_errors = [(abs(x-y)/abs(y), i) for i, (x, y) in enumerate(zip(actual, reference)) if y]
                        maximum, index = max(point_errors)
                        failures = [(i, y) for i, (x, y) in enumerate(zip(actual, reference))
                                    if y and abs(x-y)/abs(y) > Decimal('.1')]
                        item[str(d)] = dict(history_error=float(error), gate_pass=error <= Decimal('.1'),
                            nonzero_reference_native_zero_windows=zero_loss,
                            maximum_pointwise_relative_error=float(maximum), maximum_pointwise_tick=5*(index+1),
                            pointwise_gate_pass=maximum <= Decimal('.1'),
                            pointwise_failure_count=len(failures),
                            largest_failed_reference_j=str(max((abs(y) for _, y in failures), default=Decimal(0))),
                            native_zero_references=([dict(tick=5*(i+1), reference_j=str(y))
                                for i, (x, y) in enumerate(zip(actual, reference)) if not x and y]
                                if name == 'corrected' else []),
                            final_native_j=float(actual[-1]), final_be_j=float(reference[-1]))
                    else:
                        assert all(x == 0 for x in actual)
                        item[str(d)] = dict(history_error=None, gate_pass=True, exact_zero=True)
                local.append(item)
            distribution[name] = local
        # Per-packet/accounting checks use input-linked rounding bounds, never
        # an absolute physical signal floor. Distribution ratios above have none.
        ledger = {};eps = np.finfo(float).eps
        for name in ['old', 'corrected']:
            sums = [Decimal(0)]*16
            worst_gas = worst_received = worst_allocated = worst_native = Decimal(0)
            maximum_energy_relative = Decimal(0);negative = 0
            for i, row in enumerate(packets):
                actual = native[name][i];assert actual['tick'] == (i+1)*5
                for j, rec in enumerate(row):
                    q, gas_debit, prior_received = rec[:3]
                    assert sum(rec[3:]) == q
                    if q < 0:negative += 1
                    sums[j] += D(q)
                    for label, value in [('gas', gas_debit), ('capture_received', prior_received),
                                         ('received', actual['received_delta_j'][j]), ('allocated', actual['allocated_j'][j])]:
                        residual = abs(D(value)-D(q))
                        bound = D(32768*eps*abs(q))+D(64*math.ulp(q))+D(64*math.ulp(value))
                        assert residual <= bound, (name, i, j, label, residual, bound)
                        if label == 'gas':worst_gas = max(worst_gas, residual)
                        elif label == 'allocated':worst_allocated = max(worst_allocated, residual)
                        else:worst_received = max(worst_received, residual)
                    e = sum(D(v) for v in actual['energy_j'][j])
                    residual = abs(e-sums[j]);worst_native = max(worst_native, residual)
                    bound = D(32768*eps)*sum(abs(v) for v in [e, sums[j]])+D(64*math.ulp(float(sums[j])))
                    assert residual <= bound, (name, i, j, residual, bound)
                    if sums[j]:maximum_energy_relative = max(maximum_energy_relative, residual/abs(sums[j]))
            ledger[name] = dict(packet_checks=16384, microsteps=81920, negative_packets=negative,
                max_packet_gas_residual_j=float(worst_gas), max_received_residual_j=float(worst_received),
                max_allocation_residual_j=float(worst_allocated), max_cumulative_energy_residual_j=float(worst_native),
                max_cumulative_energy_relative=float(maximum_energy_relative),
                final_total_energy_j=float(sum(sums)))
    def mean(row, d):
        value = 0.
        for pair in row['energy_j']:value += pair[d]/(50/128)*8/128
        return value
    for i, row in enumerate(native['old']):
        if row['tick']%40 == 0:
            oldrow = prior[row['tick']//40]
            for d in [0, 1]:assert mean(row, d) == oldrow[f'solid{d}_k'][6]
    reference = {}
    for d in [0, 1]:
        a0 = read(a.faint/'p6-time.jsonl');a1 = read(a.faint/'p6-wrap-space.jsonl')
        expected = np.array([x[f'solid{d}_k']+y[f'solid{d}_k'] for x, y in zip(a0, a1)])
        actual = np.array([0.]+[mean(r, d) for r in native['corrected'] if r['tick']%40 == 0])
        reference[str(d)] = float(np.abs(actual-expected).sum()/np.abs(expected).sum())
    assert all(s[str(d)]['gate_pass'] for s in distribution['corrected'] for d in [0, 1])
    result = dict(gate=.1, exact_capture_history_parity=True, strict_sanitizer_replay_exact=True,
        windows=1024, patch=6, strips=list(range(96, 112)), distribution=distribution,
        ledger=ledger, corrected_given_old_q_vs_physical_reference=reference,
        full_corrected_gas_run=False, feedback=False, inputs=inputs)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v for k, v in inputs.items())
    (out/'report.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result), flush=True)


if __name__ == '__main__':main()
