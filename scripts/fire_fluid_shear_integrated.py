"""One short prescribed-shear integration checkpoint. No automatic refinement."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def read(p):return [json.loads(x) for x in p.read_text().splitlines()]


def main():
    p=argparse.ArgumentParser()
    for key in ['control','selected','output','previous']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    write(out/'plan.json',dict(duration_s=.5,gate=.1,native_memory_cap_bytes=128*1024*1024,
        cases=['control128 strict/sanitizer .05s','selected256 strict/sanitizer .05s','selected256 strict .5s'],
        cap_s=dict(control_smoke=180,selected_smoke=300,selected_sanitize=600,selected_full=1800),
        no_automatic_extended_horizon_or_accuracy_failure_rerun=True))
    identities={}
    for name,b in [('control',a.control),('selected',a.selected)]:
        identity=json.loads((b/'identity.json').read_text());assert identity['core_version']=='0.3.3'
        assert all(sha(Path(k))==v for k,v in identity['inputs'].items())
        assert all(sha(b/k)==v for k,v in identity['binaries'].items())
        identities[name]=identity;write(out/(name+'-identity.json'),identity)
        for test in ['columns-test','face-test','support-test','slab-test']:
            result=subprocess.run([b/test],capture_output=True,text=True,check=True,timeout=30)
            (out/(name+'-'+test+'.log')).write_text(result.stdout+result.stderr)
    def run(build,exe,name,nz,steps,cap):
        path=out/(name+'.jsonl');trace=out/(name+'.packets');command=list(map(str,[build/exe,128,nz,5,steps,10,trace]));start=time.monotonic()
        with path.open('x') as f,path.with_suffix('.log').open('x') as err:
            try:result=subprocess.run(command,stdout=f,stderr=err,timeout=cap);code=result.returncode
            except subprocess.TimeoutExpired:code=124
        write(path.with_suffix('.receipt.json'),dict(command=command,returncode=code,wall_s=time.monotonic()-start,cap_s=cap))
        if code:raise RuntimeError(f'{name}: refusal/failure/cap {code}; stop this checkpoint, no automatic retry')
        return read(path)
    prior=read(a.previous/'runs/repaired-full.jsonl')
    control=run(a.control,'probe','control-smoke',128,80,180)
    sanitizer=run(a.control,'probe-sanitize','control-sanitize',128,80,300)
    assert control==sanitizer and control[:-1]==prior[:3]
    assert (out/'control-smoke.packets').read_bytes()==(out/'control-sanitize.packets').read_bytes()
    assert (out/'control-smoke.packets').read_text().splitlines()[1:]==(a.previous/'runs/repaired-smoke.packets').read_text().splitlines()[1:]
    print('legacy control observations and accepted packets reproduce retained128profile exactly',flush=True)
    selected=run(a.selected,'probe','selected-smoke',256,320,300)
    sanitizer=run(a.selected,'probe-sanitize','selected-sanitize',256,320,600)
    assert selected==sanitizer and (out/'selected-smoke.packets').read_bytes()==(out/'selected-sanitize.packets').read_bytes()
    assert selected[-1]['refusal_checks']==8 and selected[-1]['native_ticks_per_second']==6400
    print('selected256 short strict/sanitizer parity and staged refusal/recovery pass',flush=True)
    full=run(a.selected,'probe','selected-full',256,3200,1800)
    assert full[:3]==selected[:-1]
    t=full[-1];assert t['steps']==3200 and t['refusal_checks']==8 and t['velocity_error']==0 and t['memory_live_bytes']==0
    assert t['configured_source_off_tick']==t['last_source_tick']==2560 and t['restriction_tick']==3128 and t['observation_ticks']==160
    assert t['fine_scalar_dt_s']==1/51200 and t['coarse_scalar_dt_s']==1/12800 and t['fine_substeps']==4
    assert [r['tick'] for r in full[:-1]]==list(range(0,3201,160))
    assert t['minimum_k']>=0 and t['maximum_k']<=.4+1e-10
    assert abs(full[-2]['source_j']-.05888671875)<1e-10
    assert max(abs(r['gas_balance_j']) for r in full[:-1])<1e-8
    for row in full[:-1]:assert abs(sum(row['local_q_j'])-row['q_j'])<1e-12
    assert all(sha(Path(k))==v for identity in identities.values() for k,v in identity['inputs'].items())
    write(out/'native-sha256.json',{p.name:sha(p) for p in out.iterdir() if p.suffix in ['.jsonl','.packets','.log']})
    print('selected256 complete through .5s; exact physical source/restriction/observation clock readback passes',flush=True)


if __name__=='__main__':main()
