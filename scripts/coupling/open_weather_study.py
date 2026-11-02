"""Finite serial live Fire–Fluid weather campaign; exact joint journals own state."""
import array,base64,hashlib,json,math,os,shutil
from pathlib import Path
from .framework import Framework
from .live_gust import retire_attempts,write
from .open_weather_cases import configuration
from .open_weather import command
from .open_weather_diagnostics import probes
from .velocity import observation,sample_mac
from .kinetic_energy import NAMES
from surface_sources.growth_fire_v1 import require
CASES=tuple((profile,16) for profile in ('none','continuous','gust','fade','reversal'))+tuple((profile,32) for profile in ('continuous','gust','reversal'))


def record(cp,c,fire):
    f=cp['fluid'];d=f['state']['data'];o=d['inert_object'];w=d['open_domain_forcing_state'];ledger=d['kinetic_ledger_j']
    gas=math.fsum(d['energy_j']);b=f['budgets']['energy_j']
    thermal=math.fsum([gas,o['gas_to_object_j'],b['outflow'],-b['inflow'],-d['input_energy_j'],-d['initial_energy_j']])
    kinetic=math.fsum([*ledger,w['external_work_j'],-d['kinetic_stored_j']])
    require(abs(thermal)<1e-9 and abs(kinetic)<1e-10*max(1,abs(w['external_work_j']),math.fsum(abs(v) for v in ledger)),
            'live weather independent thermal/mechanical accounting')
    ready=observation(cp,c,cp['evidence']['window_ticks'][1],0,height_m=c['return_channel']['sample_height_m'])
    vectors=ready['native_velocity_m_s'];held=cp['evidence'].get('held_return')
    native=fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
    points=probes(c)
    return {'revision':cp['revision'],'time_s':d['time_s'],'checkpoint_digest':cp['digest'],
        'native_state_digest':f['state']['digest'],'source_digest':cp['source']['digest'],'fire_tick':native['tick'],
        'gas_steps':d['steps'],'source_delivery':cp['evidence'].get('source_delivery'),
        'held_from_checkpoint':None if held is None else held['observation']['joint_checkpoint_digest'],
        'held_wind_digest':None if held is None else held['wind_input_digest'],
        'held_mean_x_m_s':None if held is None else math.fsum(v[0] for v in held['wind_input']['tangent_velocity_m_s'])/len(vectors),
        'ready_return_mean_x_m_s':math.fsum(v[0] for v in vectors)/len(vectors),
        'last_driver_target_strength':None if d['steps']==0 else command(c['fluid'],d['steps']-1),
        'next_driver_target_strength':command(c['fluid'],d['steps']),
        'probes':{name:{'position_m':point,'velocity_m_s':sample_mac(d['face_velocity_m_s'],c['fluid']['grid'],c['fluid']['length_m'],point,solid_floor=True,lateral_open=True)} for name,point in points.items()},
        'gas_stored_j':gas,'gas_input_j':d['input_energy_j'],'gas_smoke_kg':math.fsum(d['smoke_kg']),
        'gas_input_smoke_kg':d['input_smoke_kg'],'gas_to_object_j':o['gas_to_object_j'],
        'object_rise_k':o['temperature_k']-c['fluid']['inert_object']['initial_temperature_k']+o['temperature_correction_k'],
        'scalar_budgets':f['budgets'],'thermal_defect_j':thermal,'kinetic_defect_j':kinetic,
        'external_work_j':w['external_work_j'],'air_in_m3':w['air_in_m3'],'air_out_m3':w['air_out_m3'],
        'kinetic_stored_j':d['kinetic_stored_j'],'kinetic_stages_j':dict(zip(NAMES,ledger)),
        'max_divergence_s_inv':f['fields']['max_divergence_s_inv'],'projection_relative_residual':f['fields']['projection_relative_residual'],
        'native_peak_bytes':f['fields']['numerical_peak_bytes']}


def run_case(workers,root,profile,grid,*,budget_root,progress):
    root=Path(root);root.mkdir(parents=True,exist_ok=False)
    c=configuration(workers,profile,grid);name=profile+'-'+str(grid)
    # Fresh event identity for each live feedback history; physical source law
    # and calibration are unchanged. Never rebind a retained producer stream.
    c['source']['branch_id']='weather-'+name
    write(root/'config.json',c);attempts=root/'native-attempts';os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(attempts)
    host=Framework(c,root/'run');cp=host.initialize();rows=[record(cp,c,host.fire)]
    source=host.fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
    cal=c['source']['calibration'];fuel=math.fsum(a*b for a,b in zip(host.fire.areas(c['source']),source['fields']['fuel']))*cal['fuel_kg_per_unit_m2']
    cap=fuel*cal['energy_j_per_fuel_kg']*cal['energy_transfer_fraction'];available=0.;delivered=0.;plateau=None;files={}
    retire_attempts(attempts,root/'attempt-receipts')
    for revision in range(c['windows']):
        require(sum(p.stat().st_blocks*512 for p in Path(budget_root).rglob('*') if p.is_file())<4*1024**3
                and shutil.disk_usage(root).free>5*1024**3,'weather study storage reserve')
        progress(name,revision,'native-step')
        host.step(revision,name+'-'+str(revision));cp=host.inspect();row=record(cp,c,host.fire);rows.append(row)
        receipt=row['source_delivery'];available+=receipt['available']['energy_transferred_j'];delivered+=receipt['delivered']['energy_transferred_j']
        require(available<=cap+1e-10,'original native source fuel heat cap')
        require(row['native_peak_bytes']<=512*1024**2,'weather native allocation')
        if revision+1==c['source_delivery']['active_windows']:plateau=(row['gas_input_j'],row['gas_input_smoke_kg'])
        if revision+1>c['source_delivery']['active_windows']:
            require(not receipt['active'] and all(v==0 for v in receipt['delivered'].values()) and
                    (row['gas_input_j'],row['gas_input_smoke_kg'])==plateau,'weather source delivery cutoff')
        if any(abs(row['time_s']-t)<1e-12 for t in (.4,.8,1.2,1.6,2.4,3.2)):
            path=root/('fields-%04d.bin'%round(row['time_s']*1000))
            with path.open('xb') as out:
                for key in ('face_velocity_m_s','energy_j','smoke_kg'):array.array('d',cp['fluid']['state']['data'][key]).tofile(out)
            with path.open('rb') as inp:files[path.name]=hashlib.file_digest(inp,'sha256').hexdigest()
        write(root/'observations.json',rows);retire_attempts(attempts,root/'attempt-receipts');progress(name,revision+1,'published')
    progress(name,32,'independent-reopen');head=Framework(c,root/'run').inspect()
    require(head['digest']==cp['digest'],'weather independent joint reopen')
    require(math.isclose(delivered,rows[-1]['gas_input_j'],rel_tol=1e-12),'weather delivered source receipt')
    summary={'status':'completed','case':name,'grid':grid,'profile':profile,'head_digest':head['digest'],'independent_reopen':True,
        'source_initial_fuel_kg':fuel,'source_heat_cap_j':cap,'available_source_heat_j':available,'delivered_source_heat_j':delivered,
        'native_snapshots_sha256':files,'native_field_layout':{'dtype':'native_little_endian_float64','order':['face_velocity_m_s','energy_j','smoke_kg'],'face_count':3*grid**3+3*grid**2,'cell_count':grid**3},'final':rows[-1],'max_thermal_defect_j':max(abs(v['thermal_defect_j']) for v in rows),
        'max_kinetic_defect_j':max(abs(v['kinetic_defect_j']) for v in rows),'negative_external_work_increment':any(b['external_work_j']<a['external_work_j'] for a,b in zip(rows,rows[1:])),
        'physical_calibration_qualified':False,'spatial_convergence_qualified':False}
    write(root/'summary.json',summary);return summary
