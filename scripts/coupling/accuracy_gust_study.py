"""Fixed retained source, same physical gust; serial native nested-grid controls."""
from pathlib import Path
import array,hashlib,json,math,os,time
from .accuracy_gust_cases import configuration,CASES
from .transport_replay import ReplayRun
from .open_weather_diagnostics import probes
from .velocity import sample_mac
from surface_sources.growth_fire_v1 import require

def allocation(root):
    return sum(p.stat().st_blocks*512 for p in root.rglob("*") if p.is_file() and not p.is_symlink())

def write(path,value):
    temp=path.with_suffix(path.suffix+'.pending');temp.write_text(json.dumps(value,indent=2)+'\n');temp.replace(path)

def observation(cp,c):
    f=c['fluid'];r=cp['fluid'];d=r['state']['data'];o=d['inert_object'];n=f['grid'][0];box=f['solid_box'];lo=[round(x*n) for x in box['lower_m']];hi=[round(x*n) for x in box['upper_m']];depth=round(f['inert_object']['support_depth_m']*n)
    heat=math.fsum(d['energy_j'][(k*n+j)*n+i] for k in range(lo[2],hi[2]) for j in range(lo[1],hi[1]) for i in range(lo[0]-depth,lo[0]));volume=depth*(hi[1]-lo[1])*(hi[2]-lo[2])/n**3;props=f['properties'];state=d['open_domain_forcing_state'];budgets=r['budgets']
    thermal=math.fsum([math.fsum(d['energy_j']),o['gas_to_object_j'],budgets['energy_j']['outflow'],-budgets['energy_j']['inflow'],-d['input_energy_j'],-d['initial_energy_j']]);kinetic=math.fsum([*d['kinetic_ledger_j'],state['external_work_j'],-d['kinetic_stored_j']])
    require(abs(thermal)<1e-9 and abs(kinetic)<1e-9,'independent fixed-gust energy identities')
    return {'time_s':d['time_s'],'steps':d['steps'],'checkpoint_digest':cp['digest'],'native_state_digest':r['state']['digest'],'object_rise_k':o['temperature_k']-props['reference_temperature_k']+o['temperature_correction_k'],'support_mean_rise_k':heat/(volume*props['density_kg_m3']*props['heat_capacity_j_kg_k']),'gas_stored_j':math.fsum(d['energy_j']),'input_j':d['input_energy_j'],'input_smoke_kg':d['input_smoke_kg'],'gas_smoke_kg':math.fsum(d['smoke_kg']),'object_heat_j':o['gas_to_object_j'],'heat_outflow_j':budgets['energy_j']['outflow'],'smoke_outflow_kg':budgets['smoke_kg']['outflow'],'thermal_defect_j':thermal,'kinetic_defect_j':kinetic,'external_work_j':state['external_work_j'],'air_in_m3':state['air_in_m3'],'air_out_m3':state['air_out_m3'],'kinetic_j':d['kinetic_stored_j'],'probes':{name:sample_mac(d['face_velocity_m_s'],f['grid'],f['length_m'],p,solid_floor=True,lateral_open=True) for name,p in probes(c).items()}}

def run(output,workers,pulse,budget_root):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=False);budget_root=Path(budget_root).resolve();results={}
    write(output/'process.json',{'pid':os.getpid(),'script':str(Path(__file__).resolve()),'status':'running','cases':CASES,'root':str(output)})
    for name in CASES:
        require(allocation(budget_root)<4*1024**3,'fixed-gust total disk budget')
        c=configuration(workers,name);root=output/name;runner=ReplayRun(root,c,pulse,packet_steps=20);cp=runner.initialize();write(root/'config.json',c);rows=[observation(cp,c)];snapshots={};count=round(.2/c['fluid']['momentum_dt_s'])
        for window in range(16):
            cp=runner.advance(window,count);row=observation(cp,c);rows.append(row);write(root/'observations.json',rows)
            if window+1 in (4,8,12,16):
                d=cp['fluid']['state']['data'];raw=b''.join(array.array('d',d[key]).tobytes() for key in ['face_velocity_m_s','energy_j','smoke_kg']);p=root/f'fields-{round(row["time_s"]*1000):04d}.bin';p.write_bytes(raw);snapshots[p.name]=hashlib.sha256(raw).hexdigest()
            write(output/'progress.json',{'status':'running','case':name,'window':window+1,'time_s':row['time_s'],'completed':list(results),'allocated_bytes':allocation(budget_root)});print(name,window+1,row['time_s'],flush=True)
        head=runner.audit();require(head['digest']==cp['digest'],'independent fixed-gust reopen')
        require(math.isclose(rows[-1]['input_j'],pulse['totals']['energy_transferred_j'],rel_tol=1e-12),'fixed physical pulse heat')
        release=next(row['external_work_j'] for row in rows if abs(row['time_s']-1.2)<1e-12);require(all(row['external_work_j']==release for row in rows if row['time_s']>=1.2),'exact force-work plateau after gust release')
        summary={'status':'completed','case':name,'independent_reopen':True,'head_digest':cp['digest'],'source_pulse_digest':pulse['digest'],'native_snapshots_sha256':snapshots,'final':rows[-1]};write(root/'summary.json',summary);results[name]=summary;write(output/'results.json',results)
    write(output/'terminal.json',{'status':'numerical-complete-pending-audit','cases':results});write(output/'progress.json',{'status':'terminal-pending-analysis','completed':list(results)})
