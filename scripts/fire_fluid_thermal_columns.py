"""Local wall/solid-column qualification. No lateral solid conduction."""
import argparse, hashlib, json
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative

def metrics(rows, ref):
    assert len(rows)==len(ref)==129
    result={}
    for key in ['transport_k','near_k','tail_k','q_j','solid0_k','solid1_k','local_q_j']:
        a=np.array([r[key] for r in rows]);b=np.array([r[key] for r in ref])
        result[key]=relative(a,b)
        if key in ['transport_k','near_k','tail_k']:
            result['early_'+key]=relative(a[:61],b[:61])
        if a.ndim==2:
            # Fixed physical source-footprint and first downstream wall patches.
            for j in [1,2]:
                result[f'{key}_patch{j}']=relative(a[:,j],b[:,j])
                result[f'early_{key}_patch{j}']=relative(a[:61,j],b[:61,j])
            result[key+'_peak_patch']=relative(a.max(axis=1),b.max(axis=1))
    assert all(v is not None and np.isfinite(v) for v in result.values())
    return result

def local_patch_audit(rows, refs):
    """Do not let an aggregate pass hide a weak local failure or uncertain oracle."""
    result={}
    for key in ['solid0_k','solid1_k','local_q_j']:
        native=np.array([r[key] for r in rows])
        fine=np.array([r[key] for r in refs['time']])
        changes=[]
        for lo,hi in [('base','modes'),('modes','normal'),('normal','time')]:
            x=np.array([r[key] for r in refs[lo]])
            y=np.array([r[key] for r in refs[hi]])
            changes.append(np.abs(x-y).sum(axis=0)/np.abs(y).sum(axis=0))
        qualification=np.max(changes,axis=0)
        err=np.abs(native-fine).sum(axis=0)/np.abs(fine).sum(axis=0)
        result[key]=[dict(patch=i,x_lo_m=i/32,x_hi_m=(i+1)/32,
            relative_error=float(err[i]),max_absolute_error=float(np.abs(native-fine)[:,i].max()),
            max_reference_signal=float(np.abs(fine[:,i]).max()),reference_relative_change=float(qualification[i]),
            reference_qualified=bool(qualification[i]<.005),
            gate_pass=bool(err[i]<=.1) if qualification[i]<.005 else None) for i in range(8)]
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--edge-refinement',action='store_true');p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    refs={}
    for name,n,m,steps in [('base',256,64,5120),('modes',256,128,5120),('normal',512,128,5120),('time',512,128,10240)]:
        refs[name]=execute([build/'reference',n,m,steps,10],out/f'reference-{name}.jsonl',120)
        print('reference '+name,flush=True)
    checks={key:metrics(refs[x],refs[y]) for key,x,y in [('modes','base','modes'),('normal','modes','normal'),('time','normal','time')]}
    (out/'reference-checks.json').write_text(json.dumps(checks,indent=2)+'\n')
    assert all(v<.005 for row in checks.values() for v in row.values()), checks
    cases=[]
    matrix=[(64,64,5),(64,128,5),(64,128,10)]+([(128,64,5)] if a.edge_refinement else [])
    for nx,nz,window in matrix:
        rows=execute([build/'probe',nx,nz,window,5120,10],out/f'case-{nx}-{nz}-{window}.jsonl',180);native=rows.pop()
        assert native['terminal'] and native['steps']==5120 and native['memory_live_bytes']==0 and native['velocity_error']==0 and native['refusal_checks']==6
        assert [r['tick'] for r in rows]==list(range(0,5121,40))
        assert all(np.isfinite(v).all() for r in rows for v in r.values())
        assert max(abs(r['gas_balance_j']) for r in rows)<1e-8
        assert max(abs(r['solid_balance_j']) for r in rows)<1e-9
        assert abs(rows[-1]['source_j']-.05888671875)<1e-10
        for r in rows:
            assert abs(sum(r['local_q_j'])-r['q_j'])<1e-12
            assert abs(50/8*sum(r['solid0_k']+r['solid1_k'])-r['q_j'])<1e-12
        replay={str(d):relative([r[f'solid{d}_k'] for r in rows],[r[f'replay{d}_k'] for r in rows]) for d in (0,1)}
        assert all(v<=.1 for v in replay.values())
        m=metrics(rows,refs['time'])
        location=dict(native_patch=int(np.argmax(rows[-1]['solid0_k'])),reference_patch=int(np.argmax(refs['time'][-1]['solid0_k'])),native_peak_x_m=rows[-1]['peak_x_m'])
        row=dict(nx=nx,nz=nz,window_ticks=window,metrics=m,gates={k:v<=.1 for k,v in m.items()},hotspot=location,solid_given_native_q=replay,native=native)
        row['local_patch_audit']=local_patch_audit(rows,refs)
        row['all_local_patches_qualified']=all(v['gate_pass'] is True for values in row['local_patch_audit'].values() for v in values)
        row['hotspot_location_pass']=location['native_patch']==location['reference_patch']
        cases.append(row);print(json.dumps(row),flush=True)
        (out/'report.json').write_text(json.dumps(dict(threshold_relative=.1,cases=cases,complete=len(cases)==len(matrix),selected_case_index=1,selected_all_local_patches_qualified=(cases[1]['all_local_patches_qualified'] if len(cases)>1 else None),lateral_solid_conduction=False,full_box_qualified=False),indent=2)+'\n')

if __name__=='__main__':main()
