"""One scalar-only fixed-input gust A/B, with immutable retained default baseline."""
import array,copy,hashlib,json,math,os
from pathlib import Path
from .accuracy_gust_cases import configuration
from .accuracy_gust_study import observation,write,allocation
from .scalar_profile import FIELD,PROFILE
from .transport_replay import ReplayRun
from surface_sources.growth_fire_v1 import require


def run(output,workers,pulse,baseline,budget_root,*,resume_initialized=False):
    budget_root=Path(budget_root).resolve()
    output=Path(output).resolve()
    if not resume_initialized:output.mkdir(parents=True,exist_ok=False)
    else:require(output.is_dir() and not output.is_symlink(),'physical initialized output')
    if resume_initialized:
        for name in ('process.json','error.json'):
            src=output/name;dst=output/('initial-'+name);require(not dst.exists(),'unreplayed initialized recovery');src.rename(dst)
    write(output/'process.json',{'pid':os.getpid(),'script':str(Path(__file__).resolve()),'root':str(output),'case':'candidate64','status':'running'})
    baseline=Path(baseline).resolve();original=json.loads((baseline/'config.json').read_text())
    c=configuration(workers,'gust64');c['fluid'][FIELD]=PROFILE
    compare=copy.deepcopy(c);compare['fluid'].pop(FIELD);compare['fluid_worker']=original['fluid_worker']
    require(compare==original,'matched gust changes scalar profile and exact worker identity only')
    require(allocation(budget_root)<4*1024**3,'candidate total disk budget')
    runner=ReplayRun(output/'candidate64',c,pulse,packet_steps=20)
    if resume_initialized:
        cp=runner.audit();require(cp['revision']==0 and cp['fluid']['state']['data']['steps']==0,'only zero-step initialized recovery')
        from .live_gust import retire_attempts
        retire_attempts(runner.root/'native-attempts',runner.root/'attempt-receipts')
        write(output/'initialized-recovery.json',{'status':'resumed-exact-zero-step-head','head_digest':cp['digest'],'binding':cp['binding'],'native_reinitialization':False})
    else:cp=runner.initialize()
    root=runner.root;write(root/'config.json',c);rows=[observation(cp,c)];snapshots={}
    for window in range(16):
        cp=runner.advance(window,40);d=cp['fluid']['state']['data'];rows.append(observation(cp,c));require(d[FIELD]==PROFILE,'accepted native scalar profile');write(root/'observations.json',rows)
        if window+1 in (4,8,12,16):
            p=root/f'fields-{(window+1)*200:04d}.bin';p.write_bytes(b''.join(array.array('d',d[k]).tobytes() for k in ('face_velocity_m_s','energy_j','smoke_kg')));snapshots[p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
        write(output/'progress.json',{'status':'running','window':window+1,'time_s':d['time_s'],'allocated_bytes':allocation(budget_root)});print('candidate64',window+1,d['time_s'],flush=True)
        require(allocation(budget_root)<4*1024**3,'candidate total disk budget')
    head=runner.audit();require(head['digest']==cp['digest'],'independent candidate reopen')
    require(math.isclose(rows[-1]['input_j'],pulse['totals']['energy_transferred_j'],rel_tol=1e-12),'matched source heat')
    release=next(v['external_work_j'] for v in rows if abs(v['time_s']-1.2)<1e-12);require(all(v['external_work_j']==release for v in rows if v['time_s']>=1.2),'force work constant after gust release')
    summary={'status':'completed','independent_reopen':True,'head_digest':head['digest'],'native_snapshots_sha256':snapshots,'profile':PROFILE,'source_pulse_digest':pulse['digest'],'final':rows[-1]};write(root/'summary.json',summary);write(output/'terminal.json',{'status':'numerical-complete-pending-analysis','case':summary});write(output/'progress.json',{'status':'terminal-pending-analysis','window':16,'time_s':3.2})
