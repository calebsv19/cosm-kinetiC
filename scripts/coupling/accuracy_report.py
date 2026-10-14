"""Independent comparisons of compact production-solver accuracy probe fields."""
import json,math
from pathlib import Path
from fire_fluid_accuracy import fields
from .convergence import comparison,restrict
from .isolated_qualification import PROBES
from .velocity import sample_mac
from .solid_box import topology,blocked_faces
from surface_sources.growth_fire_v1 import require

def load(root,name,n):
    path=root/name;summary=path/'summary.json'
    if not summary.exists():summary=path/'native-summary.json'
    value=json.loads(summary.read_text())
    require(value['grid']==n and value['time_s']==3.2,'native result identity')
    f=fields(path/'fields.bin',n)
    require(len(f['face_velocity_m_s'])==3*n**3+3*n*n,'native field dimensions')
    require(all(math.isfinite(v) for vector in f.values() for v in vector),'finite native fields')
    require(all(v>=0 for key in ('energy_j','smoke_kg') for v in f[key]),'nonnegative native scalars')
    return value,f

def common(f,n):return {key:restrict(f[key],[n]*3,[8]*3) for key in ('energy_j','smoke_kg')}

def probe_checks(f,n,box):
    config={'grid':[n]*3,'length_m':[1.]*3,'boundary_policy':{'vertical':'solid_bottom_open_top'}}
    if box:config['solid_box']={'schema':'physics_sim_stationary_box/v1','lower_m':[.375]*3,'upper_m':[.625]*3,'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    mask=topology(config);blocked=blocked_faces(config,mask)
    if any(f['face_velocity_m_s'][q]!=0 for q in blocked) or any(solid and (f['energy_j'][q]!=0 or f['smoke_kg'][q]!=0) for q,solid in enumerate(mask)):
        raise ValueError('solid exclusion')
    return {'solid_cells':sum(mask),'blocked_faces':len(blocked),'max_blocked_normal_velocity_m_s':0.,
            'wind_probes':{name:sample_mac(f['face_velocity_m_s'],[n]*3,[1.]*3,point,solid_floor=True,lateral_open=True) for name,point in PROBES.items()}}

def report(root,prior):
    root=Path(root);prior=Path(prior)
    time_fields=[load(root,name,32)[1] for name in ('no-box-32-dt005','no-box-32-dt0025','no-box-32-dt00125')]
    temporal=[comparison(time_fields[i],time_fields[i+1],[32]*3,[32]*3) for i in (0,1)]
    spatial={};checks={};native_parity={}
    for name,box in (('no-box',False),('box',True)):
        c=json.loads((prior/('matched-'+name+'-16/fluid.json')).read_text())['fields']
        p32='box-32-csr' if box else 'no-box-32-dt005';p64='box-64-csr' if box else 'no-box-64-dt005'
        s32,f32=load(root,p32,32);s64,f64=load(root,p64,64)
        pairs=[comparison(common(c,16),common(f32,32),[8]*3,[8]*3),comparison(common(f32,32),common(f64,64),[8]*3,[8]*3)]
        spatial[name]={'common_8_partition_pairs':pairs,'native_32_partition_fine_pair':comparison(f32,f64,[32]*3,[64]*3)}
        checks[name]={'32':dict(native=s32,**probe_checks(f32,32,box)), '64':dict(native=s64,**probe_checks(f64,64,box))}
        old_probes=json.loads((prior/('matched-'+name+'-16/checks.json')).read_text())['wind_probes']
        vectors=[[x for row in old_probes.values() for x in row['velocity_m_s']]] + [
            [x for row in checks[name][str(n)]['wind_probes'].values() for x in row] for n in (32,64)]
        delta=[math.sqrt(math.fsum((a-b)**2 for a,b in zip(vectors[i],vectors[i+1]))) for i in (0,1)]
        spatial[name]['probe_velocity_difference_l2_m_s']=delta
        spatial[name]['probe_differences_decrease']=delta[1]<delta[0]
        for native in (s32,s64):
            require(abs(native['heat_balance_j'])<1e-9 and abs(native['smoke_balance_kg'])<1e-15
                    and native['divergence']<1e-8 and native['pressure_residual']<1e-11
                    and native['peak_temperature_k']<=330,'native accuracy invariants')
        old=json.loads((prior/('matched-'+name+'-32/fluid.json')).read_text())['fields']
        native_parity[name]={key:max(abs(a-b) for a,b in zip(f32[key],old[key])) for key in f32}
    source=json.loads((root/'no-box-64-dt005/summary.json').read_text())['source_frames_sha256']
    temporal_pass=all(pair[key]['relative_l1']<.01 for pair in temporal for key in ('energy_j','smoke_kg')) and all(temporal[1][key]['relative_l1']/temporal[0][key]['relative_l1']<.6 for key in ('energy_j','smoke_kg'))
    spatial_pass=all(value['probe_differences_decrease'] and value['common_8_partition_pairs'][1][key]['relative_l1']<.1 and value['common_8_partition_pairs'][1][key]['relative_l1']<value['common_8_partition_pairs'][0][key]['relative_l1'] for value in spatial.values() for key in ('energy_j','smoke_kg'))
    return {'schema':'fire_fluid_source_transport_accuracy/v1','status':'coarse_observation_gate_passed' if temporal_pass and spatial_pass else 'accuracy_unqualified',
            'temporal_gate_passed':temporal_pass,'spatial_gate_passed':spatial_pass,'time_steps_s':[.005,.0025,.00125],
            'spatial_grids':[16,32,64],'common_observation_grid':[8]*3,'spatial_threshold_relative_l1':.1,
            'temporal_threshold_relative_l1':.01,'source_frames_sha256':source,'time_refinement':temporal,'grid_refinement':spatial,
            'native_checks':checks,'probe_vs_prior_worker_max_absolute':native_parity,
            'physical_calibration':False,'thermal_feedback':False,'local_surface_heat_transfer_qualified':False,
            'limits':'common 8-cubed observation gate only; native fine heat detail remains above 10%; no ignitable object admission'}
