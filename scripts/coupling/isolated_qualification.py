"""Centered isolated plume and obstacle; fixed physical source/domain comparisons."""
import copy
import math
import shutil
from .box_qualification import configuration as previous,write,spatial
from .framework import Framework
from .workbench import read_frames
from .allocation import allocate_window
from .open_boundaries import layout,lateral
from .solid_box import topology,blocked_faces,point_outside
from .velocity import sample_mac
from .scenario import metrics
from open_atmosphere import run,SCHEMA
from surface_sources.growth_fire_v1 import require,sealed

PROBES={'below':[.5,.5,.3125],'above':[.5,.5,.6875],'left':[.3125,.5,.5],'right':[.6875,.5,.5],'front':[.5,.3125,.5],'back':[.5,.6875,.5]}

def configuration(workers,n=8,box=False,extent=1.,duration=3.2):
    config=previous(workers,grid=n,box=False,duration=duration)
    source=config['source'];shift=(extent-1)/2;source['origin_m']=[.25+shift,.25+shift,0];source['cell_size_m']=.5/7
    source['evolution']['ignition_rectangle_m']=[.125,.125,.375,.375]
    f=config['fluid'];f['boundary_policy']['horizontal']='ambient_open_xy';f['transport_scheme']='muscl_minmod'
    f['resource_limits']={'max_cells':262144,'scalar_work_cells':1000000000,'cache_pressure_operator':True}
    if n>32:
        # Fine qualification needs the admitted cache/setup and full JSON field
        # envelopes. This changes resource admission, never physical tolerances.
        f['resource_limits'].update(numerical_bytes=512*1024*1024,
                                   native_request_bytes=256*1024*1024,
                                   native_output_bytes=256*1024*1024)
    # Expanded domain adds equal margins to both sides; source/probes translate together.
    nz=round(n/extent);f['grid']=[n,n,nz];f['length_m']=[extent,extent,1.]
    count=n*n*nz;f['initial_face_velocity_m_s']=[0.]*layout(f)[0];f['initial_energy_j']=[0.]*count;f['initial_smoke_kg']=[0.]*count
    config['receiver'].update(spacing_m=[extent/n]*3,dimensions=f['grid'],fluid_mask=[1]*(n*n),injection_fluid_mask=[1]*(n*n*round(.125*n/extent)))
    if box:f['solid_box']={'schema':'physics_sim_stationary_box/v1','lower_m':[.375+shift,.375+shift,.375],'upper_m':[.625+shift,.625+shift,.625],'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    return config

def checks(config,result):
    f=config['fluid'];fields=result['fields'];mask=topology(f);blocked=blocked_faces(f,mask)
    maximum=max((abs(fields['face_velocity_m_s'][q]) for q in blocked),default=0.)
    require(maximum==0 and all(not solid or all(fields[key][q]==0 for key in ('energy_j','smoke_kg','pressure_pa')) for q,solid in enumerate(mask)),'isolated solid exclusion')
    probes={}
    for name,original in PROBES.items():
        shift=config['source']['origin_m'][0]-.25;point=[original[0]+shift,original[1]+shift,original[2]]
        point_outside(f,point);probes[name]={'position_m':point,'velocity_m_s':sample_mac(fields['face_velocity_m_s'],f['grid'],f['length_m'],point,solid_floor=True,lateral_open=True)}
    return {'blocked_faces':len(blocked),'max_blocked_normal_velocity_m_s':maximum,'solid_cells':sum(mask),'wind_probes':probes,'budgets':result['budgets'],'boundary_outflows':{r['boundary']:r['smoke_kg_outflow']['total'] for r in result['boundary_receipts']},'max_divergence_s_inv':fields['max_divergence_s_inv'],'projection_relative_residual':fields['projection_relative_residual']}

def coupled(workers,root,box):
    config=configuration(workers,box=box);write(root/'config.json',config)
    host=Framework(config,root/'store');host.initialize();host.step(0,'window-0');before=host.path.read_bytes()
    host.step(0,'window-0');require(host.path.read_bytes()==before,'same-operation replay')
    def fail(_):raise RuntimeError('deliberate publication failure')
    try:host.step(1,'window-1',before_publish=fail)
    except RuntimeError:pass
    else:raise ValueError('failure hook missing')
    require(host.path.read_bytes()==before,'accepted state rollback')
    restart=root/'restart';restart.mkdir();shutil.copy2(host.path,restart/'joint.sqlite3');other=Framework(config,restart)
    require(host.step(1,'window-1')==other.step(1,'window-1'),'exact coupled recovery')
    for revision in range(2,config['windows']):host.step(revision,'window-'+str(revision))
    cp=host.inspect();rows=read_frames(host);write(root/'head.json',cp)
    report={'metrics':metrics(cp),'checks':checks(config,cp['fluid']),'replay':True,'recovery':True,'rollback':True}
    write(root/'checks.json',report);return host,rows,report

def frozen(workers,root,frames,schedule,n,box,extent=1.):
    config=configuration(workers,n,box,extent);state=None;batch=[]
    for frame,window in zip(frames,schedule):
        if extent!=1:
            # Translation changes coordinates only; all per-node source amounts stay exact.
            frame=copy.deepcopy(frame);frame['config']=copy.deepcopy(config['source']);frame['surface']['origin_m']=list(config['source']['origin_m']);frame=sealed(frame)
        policy={'schema':'physics_sim_surface_source_policy/v1','consumer_id':'isolated-refinement','model_id':'matched-frozen-fire','source_config':config['source'],'producer':frame['producer'],'stream_start_tick':0,'receiver':config['receiver']}
        batch.extend(allocate_window(frame,policy,window))
        if len(batch)>=80:
            result=run(dict(config['fluid'],schema=SCHEMA,state=state,steps=batch),workers[2]);state=result['state'];batch=[]
    if batch:result=run(dict(config['fluid'],schema=SCHEMA,state=state,steps=batch),workers[2])
    write(root/'config.json',config);write(root/'fluid.json',result);report=checks(config,result);write(root/'checks.json',report)
    return result,report
