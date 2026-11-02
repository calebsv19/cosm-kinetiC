"""Bounded new native inlet histories; reuse Framework atomic publication."""
from fractions import Fraction
import base64,json,math,sqlite3,time
from pathlib import Path
from .scenario import baseline
from .qualification import case
from .open_boundaries import layout
from .framework import Framework,load_fire
from fire_fluid_spatial import ground_configuration
from fire_fluid_regression import qualify
from surface_sources.growth_fire_v1 import require
PROFILES=('zero','continuous-positive','continuous-negative','abrupt-gust','ramped-gust')

def configuration(workers,profile,feedback=True,*,half=False,box=False):
    require(profile in PROFILES,'wind comparison profile')
    c=ground_configuration(baseline(*workers,nodes=17,grid=8,
        fire_denominator=100 if half else 50,fluid_denominator=400 if half else 200))
    c['return_channel']={'schema':'fire_fluid_prior_velocity_hold/v1','sample_height_m':.125}
    c=case(c,height=.125,exchange=Fraction(1,10),duration=Fraction(16,5),wind_scale=0,domain_height=1.)
    f=c['fluid'];f['boundary_policy']['horizontal']='ambient_open_xy'
    f['initial_face_velocity_m_s']=[0.]*layout(f)[0]
    if box:f['solid_box']={'schema':'physics_sim_stationary_box/v1','lower_m':[.375]*3,'upper_m':[.625]*3,
        'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    times={'zero':[(0,0)],'continuous-positive':[(0,.5)],'continuous-negative':[(0,.5)],
        'abrupt-gust':[(0,0),(.4,.5),(1.2,0)],
        'ramped-gust':[(0,0),(.4,0),(.8,.5),(1.2,.5),(1.6,0)]}[profile]
    denominator=400 if half else 200
    knots=[]
    for t,speed in times:
        step=Fraction(str(t))*denominator;require(step.denominator==1,'wind event/native clock alignment')
        knots.append({'step':step.numerator,'speed_m_s':speed})
    f['wind_forcing']={'schema':'physics_sim_driven_x_inlet/v1','inlet':'x_max' if profile=='continuous-negative' else 'x_min',
        'ramp_height_m':.25,'interpolation':'linear' if profile=='ramped-gust' else 'hold','knots':knots}
    c['source']['origin_m']=[.125,.125,0]
    c['source']['evolution']['ignition_rectangle_m']=[.25,.25,.5,.5]
    c['source']['evolution']['model']='physical_rates_heat_outflow_v1'
    if not feedback:c.pop('return_channel');c['source']['evolution']['model']='physical_rates_v1'
    return c

def frames(root):
    with sqlite3.connect((Path(root)/'run/joint.sqlite3').resolve().as_uri()+'?mode=ro',uri=True) as db:
        return [json.loads(v[0]) for v in db.execute('select payload from checkpoints order by revision')]

def observations(config,rows):
    fire=load_fire(config['growth_root']);areas=fire.areas(config['source']);nx=config['source']['width']
    positions=[config['source']['origin_m'][0]+i%nx*config['source']['cell_size_m'] for i in range(len(areas))]
    result=[]
    for index,cp in enumerate(rows):
        native=fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))['fields']
        weights=[a*h for a,h in zip(areas,native['heat'])];held=cp['evidence'].get('held_return')
        if held:
            require(held['observation']['joint_checkpoint_digest']==rows[index-1]['digest'],'accepted-prior wind identity')
            require(cp['source']['operation']['wind_digest']==held['wind_input_digest'],'native Fire consumed held wind')
        f=cp['fluid'];wind=f['wind_observation']
        result.append({'revision':cp['revision'],'time_s':f['fields']['time_s'],
            'last_command_x_m_s':wind['last_target_velocity_x_m_s'],
            'next_command_x_m_s':wind['next_target_velocity_x_m_s'],
            'applied_mean_wind_x_m_s':0 if held is None else math.fsum(v[0] for v in held['wind_input']['tangent_velocity_m_s'])/len(areas),
            'fire_heat_centroid_x_m':math.fsum(x*w for x,w in zip(positions,weights))/math.fsum(weights),
            'gas_energy_j':math.fsum(f['fields']['energy_j']),'gas_smoke_kg':math.fsum(f['fields']['smoke_kg']),
            'energy_outflow_j':f['budgets']['energy_j']['outflow'],
            'smoke_outflow_kg':f['budgets']['smoke_kg']['outflow'],
            'air_in_m3':wind['air_in_m3'],'air_out_m3':wind['air_out_m3'],'inlet_volume_m3':wind['inlet_volume_m3'],
            'joint_digest':cp['digest']})
    return result

def write(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')

def run_suite(workers,output):
    output=Path(output);output.mkdir(parents=True,exist_ok=False)
    specs=[(profile+'-'+toggle,profile,toggle=='on',False,False) for profile in PROFILES for toggle in ('off','on')]
    specs += [('continuous-positive-half','continuous-positive',True,True,False),
              ('continuous-positive-box','continuous-positive',True,False,True)]
    reports={};data={}
    for label,profile,on,half,box in specs:
        require(sum(p.stat().st_size for p in output.parent.rglob('*') if p.is_file())<1_800_000_000,'new-output budget before next case')
        write(output.parent/'progress.json',{'stage':'coupled wind','case':label,'status':'running'})
        c=configuration(workers,profile,on,half=half,box=box);root=output/label
        start=time.monotonic();report=qualify(c,root)
        report['elapsed_s']=time.monotonic()-start;write(root/'summary.json',report)
        rows=frames(root);data[label]=observations(c,rows);write(root/'observations.json',data[label]);reports[label]=report
        print(label,data[label][-1]['fire_heat_centroid_x_m'],flush=True)
    # First Fire window sees the initially still gas even for an immediate inlet.
    a=frames(output/'continuous-positive-on');b=frames(output/'continuous-positive-off')
    require(a[1]['source']['frame']['cells']==b[1]['source']['frame']['cells'],'inlet looked ahead into first Fire window')
    for label,direction in (('continuous-positive-on',1),('continuous-negative-on',-1)):
        require(direction*(data[label][-1]['fire_heat_centroid_x_m']-.5)>.01,'signed sustained Fire response')
    reference=data['continuous-positive-on'][-1]['gas_energy_j']
    sensitivity=abs(data['continuous-positive-half'][-1]['gas_energy_j']/reference-1)
    require(sensitivity<=.1,'driven-wind timestep-half screen exceeds10percent')
    abrupt=data['abrupt-gust-on'];ramped=data['ramped-gust-on']
    require(math.isclose(abrupt[-1]['inlet_volume_m3'],ramped[-1]['inlet_volume_m3'],rel_tol=1e-12),'gusts must have matched integrated inlet flow')
    for label in ('abrupt-gust-on','ramped-gust-on'):
        require(data[label][-1]['next_command_x_m_s']==0,'gust did not turn off')
        require(abs(data[label][-1]['applied_mean_wind_x_m_s'])<max(abs(v['applied_mean_wind_x_m_s']) for v in data[label]),'gas did not relax after gust')
    no=frames(output/'continuous-positive-on')[-1]['fluid']['fields']['face_velocity_m_s']
    yes=frames(output/'continuous-positive-box')[-1]['fluid']['fields']['face_velocity_m_s']
    difference=math.sqrt(math.fsum((a-b)**2 for a,b in zip(no,yes)))
    require(difference>1e-6,'obstacle does not alter driven flow')
    write(output.parent/'summary.json',{'status':'passed','cases':reports,'time_half_energy_relative_change':sensitivity,
        'box_velocity_field_difference_l2_m_s':difference,'gust_inlet_volume_m3':abrupt[-1]['inlet_volume_m3'],
        'first_window_no_lookahead':True,'scope':'new native prescribed inlet; bounded 8cubed warm-flow behavior, not spatial/hot-fire qualification'})
    write(output.parent/'progress.json',{'stage':'coupled wind','status':'complete','cases':len(specs)})
