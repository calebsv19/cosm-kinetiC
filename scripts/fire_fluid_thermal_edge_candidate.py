"""Bounded native candidate check reusing immutable frozen reference snapshots."""
import argparse, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from fire_fluid_thermal_edge import snapshot, compare
from fire_fluid_thermal_transport import execute


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--references',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=a.build.resolve();root=a.references.resolve()
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    assert '-DPHYSICS_SIM_OPEN_EDGE_CANDIDATE=1' in identity['flags']
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    inputs={};cases=[]
    for name in ['candidate-test','candidate-test-sanitize','columns-test','face-test','support-test']:
        if name=='columns-test':
            result=subprocess.run([build/name],capture_output=True,text=True,timeout=30,check=True)
            (out/(name+'.log')).write_text(result.stdout+result.stderr)
        else:execute([build/name],out/(name+'.jsonl'),30)
    for tick in [2400,5120]:
        for nx,nz in [(64,64),(128,64),(64,128),(128,128)]:
            file=f'snapshot-{tick}-{nx}-{nz}.bin'
            refs={k:snapshot(root/k/file,nx,nz) for k in ['coarse','fine','time','modes']}
            for k in refs:
                path=root/k/file;inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
            row=execute([build/'probe',nx,nz,root/'time'/file,.003125,'mc'],out/f'probe-{tick}-{nx}-{nz}.jsonl',15)[0]
            assert row['native_euler_flux_parity']<1e-7 and row['live_bytes']==0 and row['clipped_gas_j']==0
            assert row['exchange_pair_max_relative']<1e-5 and row['exchange_balance_j']<1e-12
            aa=np.array(row['advection_w']).reshape(2,nx);dd=np.array(row['diffusion_w']).reshape(2,nx)
            patches=[]
            for band in [0,1]:
                for patch in [0,1,2,3,4,7]:
                    faces=[patch*nx//8,((patch+1)*nx//8)%nx];parts={}
                    for kind in ['advection','diffusion','total']:
                        def values(r):
                            return (r['adv'] if kind=='advection' else r['diff'] if kind=='diffusion' else r['adv']+r['diff'])[band,faces]
                        actual=(aa if kind=='advection' else dd if kind=='diffusion' else aa+dd)[band,faces]
                        metric=compare(actual,values(refs['time']))
                        change=max(compare(values(refs[x]),values(refs[y]))['relative'] for x,y in [('coarse','fine'),('modes','fine'),('fine','time')])
                        metric.update(reference_change=change,reference_qualified=change<.005,gate_pass=(metric['relative']<=.1 if change<.005 else None))
                        parts[kind]=metric
                    patches.append(dict(patch=patch,band=band,**parts))
            cases.append(dict(tick=tick,nx=nx,nz=nz,patches=patches,parity=row['native_euler_flux_parity'],peak_bytes=row['peak_bytes']))
    execute([build/'probe-sanitize',128,128,root/'time'/'snapshot-5120-128-128.bin',.003125,'mc'],out/'probe-sanitize.jsonl',30)
    selected=[c for c in cases if c['nx']==128 and c['nz']==64]
    admitted=all(p['total']['gate_pass'] is not False for c in selected for p in c['patches'])
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,reference_threshold=.005,inputs=inputs,cases=cases,selected_trajectory_admitted=admitted,unqualified_reference_is_not_pass=True),indent=2)+'\n')
    print(json.dumps(dict(selected_trajectory_admitted=admitted,cases=len(cases))))

if __name__=='__main__':main()
