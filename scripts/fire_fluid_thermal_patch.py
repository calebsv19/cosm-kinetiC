"""Bounded advected pulse: separate transport, wall exchange and solid replay."""
import argparse, hashlib, json
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative

GAS = ['transport_k', 'near_k', 'tail_k']
SOLID = ['solid0_k', 'solid1_k', 'q_j']

def errors(rows, ref, coupled=True):
    assert len(rows) == len(ref) == 129
    keys = GAS + (SOLID if coupled else [])
    result = {k: relative([r[k] for r in rows], [r[k] for r in ref]) for k in keys}
    for k in GAS:
        result['early_' + k] = relative([r[k] for r in rows[:61]], [r[k] for r in ref[:61]])
    assert all(v is not None and np.isfinite(v) for v in result.values())
    return result

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); build = a.build.resolve(); out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    identity = json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest() == v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    refs = {}
    for name,n,m,steps,h in [('base',256,64,5120,10), ('modes',256,128,5120,10),
                            ('normal',512,128,5120,10), ('time',512,128,10240,10),
                            ('zero',512,128,10240,0)]:
        refs[name] = execute([build/'reference',n,m,steps,h], out/f'reference-{name}.jsonl',120)
        print('reference '+name, flush=True)
    checks = dict(modes=errors(refs['base'],refs['modes']),
                  normal=errors(refs['modes'],refs['normal']),
                  time=errors(refs['normal'],refs['time']))
    (out/'reference-checks.json').write_text(json.dumps(checks,indent=2)+'\n')
    assert all(v < .005 for row in checks.values() for v in row.values()), checks
    cases = []
    for nx,nz,window,h in [(32,64,5,0),(64,64,5,0),(64,64,5,10),(64,128,5,10),(64,128,10,10)]:
        rows = execute([build/'probe',nx,nz,window,5120,h],out/f'case-{nx}-{nz}-{window}-{h}.jsonl',180)
        native = rows.pop()
        assert native['terminal'] and native['steps']==5120 and native['memory_live_bytes']==0 and native['velocity_error']==0 and native['refusal_checks']==4
        assert [r['tick'] for r in rows]==list(range(0,5121,40))
        assert all(np.isfinite(list(r.values())).all() for r in rows)
        assert max(abs(r['gas_balance_j']) for r in rows)<1e-8
        assert max(abs(r['solid_balance_j']) for r in rows)<1e-9
        assert abs(rows[-1]['source_j'] - .05888671875)<1e-10
        if not h: assert all(r[k]==0 for r in rows for k in SOLID)
        metrics = errors(rows,refs['time' if h else 'zero'],bool(h))
        replay = {k:relative([r[f'solid{k}_k'] for r in rows],[r[f'replay{k}_k'] for r in rows]) for k in (0,1)} if h else {}
        assert all(v is not None and v <= .1 for v in replay.values()), replay
        case = dict(nx=nx,nz=nz,window_ticks=window,h=h,metrics=metrics,gates={k:v<=.1 for k,v in metrics.items()},solid_given_native_q=replay,native=native)
        cases.append(case); print(json.dumps(case),flush=True)
        (out/'report.json').write_text(json.dumps(dict(threshold_relative=.1,cases=cases,complete=len(cases)==5,selected_case_index=3,selected_case_pass=(all(cases[3]['gates'].values()) if len(cases)>3 else None),full_box_qualified=False),indent=2)+'\n')

if __name__ == '__main__': main()
