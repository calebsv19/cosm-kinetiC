"""Reference-field flux and contact isolation; never changes the 10% gate."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute

def snapshot(path,nx,nz):
    v=np.fromfile(path,dtype=np.float64);assert v.size==nx*nz+6*nx
    q=nx*nz
    return dict(gas=v[:q].reshape(nz,nx),solid=v[q:q+nx],wall=v[q+nx:q+2*nx],adv=v[q+2*nx:q+4*nx].reshape(2,nx),diff=v[q+4*nx:].reshape(2,nx))

def compare(actual,expected):
    actual=np.asarray(actual);expected=np.asarray(expected)
    absolute=float(np.abs(actual-expected).sum());scale=float(np.abs(expected).sum())
    return dict(relative=absolute/scale if scale else None,absolute=absolute,reference_scale=scale)

def qualify_cases(cases, out):
    cache={}
    for c in cases:
        key=(c['tick'],c['nx'],c['nz'])
        if key not in cache:
            name=f'snapshot-{key[0]}-{key[1]}-{key[2]}.bin'
            cache[key]={k:snapshot(out/k/name,key[1],key[2]) for k in ['coarse','fine','time','modes']}
        refs=cache[key]
        for p in c['patches']:
            wall=p['wall_sampling'];wall['reference_qualified']=wall['reference_change']<.005
            wall['gate_pass']=(wall['relative']<=.1) if wall['reference_qualified'] else None
            p['exchange_kernel']['gate_pass']=p['exchange_kernel']['relative']<=.1
            faces=[p['patch']*c['nx']//8,((p['patch']+1)*c['nx']//8)%c['nx']]
            for b in p['lateral']:
                for kind in ['advection','diffusion','total']:
                    def values(r):
                        if kind=='advection':return r['adv'][b['band'],faces]
                        if kind=='diffusion':return r['diff'][b['band'],faces]
                        return r['adv'][b['band'],faces]+r['diff'][b['band'],faces]
                    change=max(compare(values(refs[x]),values(refs[y]))['relative'] for x,y in [('coarse','fine'),('modes','fine'),('fine','time')])
                    v=b[kind];v['reference_change']=change;v['reference_qualified']=change<.005
                    v['gate_pass']=(v['relative']<=.1) if v['reference_qualified'] else None

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((build/'identity.json').read_text());assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    for name,n,m,steps in [('coarse',256,128,10240),('fine',512,128,10240),('time',512,128,20480),('modes',512,64,10240)]:
        target=out/name;target.mkdir()
        execute([build/'reference',n,m,steps,10,target],out/f'reference-{name}.jsonl',120)
        print('reference '+name,flush=True)
    results=[]
    for tick in [2400,5120]:
        for nx,nz in [(64,64),(128,64),(64,128),(128,128)]:
            name=f'snapshot-{tick}-{nx}-{nz}.bin'
            ref=snapshot(out/'time'/name,nx,nz)
            peers={k:snapshot(out/k/name,nx,nz) for k in ['coarse','fine','modes']}
            for mode in ['mc','minmod']:
                for dt in [.003125,.000001]:
                    row=execute([build/'probe',nx,nz,out/'time'/name,dt,mode],out/f'probe-{tick}-{nx}-{nz}-{mode}-{dt}.jsonl',15)[0]
                    assert row['live_bytes']==0 and row['native_euler_flux_parity']<1e-7
                    assert row['exchange_pair_max_relative']<1e-5 and row['exchange_balance_j']<1e-12
                    wall=np.array(row['wall_instant_w']);actual=np.array(row['wall_native_j']);exact=np.array(row['wall_exact_frozen_j'])
                    patches=[]
                    for patch in [0,1,2,3,4,7]:
                        sl=slice(patch*nx//8,(patch+1)*nx//8)
                        walls=compare([wall[sl].sum()],[ref['wall'][sl].sum()])
                        pairs=compare(actual[sl],exact[sl])
                        walls['reference_change']=max(compare([peers[x]['wall'][sl].sum()],[peers[y]['wall'][sl].sum()])['relative'] for x,y in [('coarse','fine'),('modes','fine')])
                        walls['reference_change']=max(walls['reference_change'],compare([peers['fine']['wall'][sl].sum()],[ref['wall'][sl].sum()])['relative'])
                        lateral=[]
                        for band in [0,1]:
                            faces=[patch*nx//8,((patch+1)*nx//8)%nx]
                            aa=np.array(row['advection_w']).reshape(2,nx)[band,faces];dd=np.array(row['diffusion_w']).reshape(2,nx)[band,faces]
                            exact_a=ref['adv'][band,faces];exact_d=ref['diff'][band,faces]
                            change=max(compare(peers[x]['adv'][band,faces]+peers[x]['diff'][band,faces],peers[y]['adv'][band,faces]+peers[y]['diff'][band,faces])['relative'] for x,y in [('coarse','fine'),('modes','fine')])
                            change=max(change,compare(peers['fine']['adv'][band,faces]+peers['fine']['diff'][band,faces],exact_a+exact_d)['relative'])
                            lateral.append(dict(band=band,advection=compare(aa,exact_a),diffusion=compare(dd,exact_d),total=compare(aa+dd,exact_a+exact_d),reference_change=change,native_faces_w=(aa+dd).tolist(),reference_faces_w=(exact_a+exact_d).tolist()))
                        patches.append(dict(patch=patch,wall_sampling=walls,exchange_kernel=pairs,lateral=lateral))
                    results.append(dict(tick=tick,nx=nx,nz=nz,mode=mode,dt=dt,patches=patches,clipped_gas_j=row['clipped_gas_j'],negative_min_k=row['negative_min_k'],native_flux_parity=row['native_euler_flux_parity'],pair_max_relative=row['exchange_pair_max_relative'],peak_bytes=row['peak_bytes']))
                    print(f'probe {tick} {nx} {nz} {mode} {dt}',flush=True)
    qualify_cases(results,out)
    (out/'report.json').write_text(json.dumps(dict(threshold_relative=.1,reference_threshold=.005,cases=results,complete=True,trajectory_qualified=False),indent=2)+'\n')

if __name__=='__main__':main()
