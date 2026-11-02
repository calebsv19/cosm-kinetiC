"""One selected candidate trajectory after frozen total-flux admission."""
import argparse, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_thermal_columns import metrics, local_patch_audit
from fire_fluid_sharp_transport import relative


def main():
    p=argparse.ArgumentParser()
    for flag in ['build','frozen','references','output']:p.add_argument('--'+flag,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    admission=json.loads(a.frozen.read_text());assert admission['selected_trajectory_admitted']
    build=a.build.resolve();identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    assert '-DPHYSICS_SIM_OPEN_EDGE_CANDIDATE=1' in identity['flags']
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    inputs={str(a.frozen.resolve()):hashlib.sha256(a.frozen.read_bytes()).hexdigest()}
    refs={}
    for k in ['base','modes','normal','time']:
        f=a.references/f'reference-{k}.jsonl';refs[k]=[json.loads(x) for x in f.read_text().splitlines()]
        inputs[str(f.resolve())]=hashlib.sha256(f.read_bytes()).hexdigest()
    for name in ['columns-test','face-test','support-test','slab-test']:
        p=subprocess.run([build/name],capture_output=True,text=True,timeout=30,check=True)
        (out/(name+'.log')).write_text(p.stdout+p.stderr)
    execute([build/'probe-sanitize',128,64,5,80,10],out/'sanitize-smoke.jsonl',60)
    rows=execute([build/'probe',128,64,5,5120,10],out/'case-128-64-5.jsonl',180);native=rows.pop()
    assert native['terminal'] and native['steps']==5120 and native['memory_live_bytes']==0 and native['velocity_error']==0 and native['refusal_checks']==6
    assert [r['tick'] for r in rows]==list(range(0,5121,40))
    assert all(np.isfinite(v).all() for r in rows for v in r.values())
    gas=max(abs(r['gas_balance_j']) for r in rows);solid=max(abs(r['solid_balance_j']) for r in rows)
    assert gas<1e-8 and solid<1e-9 and abs(rows[-1]['source_j']-.05888671875)<1e-10
    for r in rows:
        assert abs(sum(r['local_q_j'])-r['q_j'])<1e-12
        assert abs(50/8*sum(r['solid0_k']+r['solid1_k'])-r['q_j'])<1e-12
    replay={str(d):relative([r[f'solid{d}_k'] for r in rows],[r[f'replay{d}_k'] for r in rows]) for d in (0,1)}
    assert all(v<=.1 for v in replay.values())
    m=metrics(rows,refs['time']);patches=local_patch_audit(rows,refs)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    result=dict(threshold=.1,nx=128,nz=64,window=5,inputs=inputs,native=native,metrics=m,gates={k:v<=.1 for k,v in m.items()},local_patch_audit=patches,solid_given_native_q=replay,gas_balance_max_j=gas,solid_balance_max_j=solid,all_local_patches_qualified=all(v['gate_pass'] is True for values in patches.values() for v in values))
    (out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))

if __name__=='__main__':main()
