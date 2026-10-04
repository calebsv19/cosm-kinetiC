#!/usr/bin/env python3
"""Repeatable local S3 experiments through the same agent session contract.
Writes immutable per-case inputs, worker identity, samples and comparisons.
A successful experiment is not a declaration that CFD is qualified.
"""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import sys
import time
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent/'agent_session'))
from service import Service, SessionError, atomic
from qualification import FLUIDS, reference
from wake_qualification import steady_screen, DEFAULT_CRITERIA
from preview import image_content


def await_receipt(s, run, revision, action, cid):
    deadline=time.monotonic()+120
    while time.monotonic()<deadline:
        out=s.run_control(run,cid,action,revision,wait_ms=5000)
        if out['status']!='pending':
            if out['status']!='applied': raise RuntimeError(out)
            return out
    raise TimeoutError('control deadline exceeded')


def sample(s, run, name, position):
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        out=s.run_sample(run,name,plane='YZ',position=position,resolution=64,field='vx',wait_ms=5000)
        if out.get('status')!='pending': return out
    raise TimeoutError('sample deadline exceeded')


def wake_profile(p, speed, diameter):
    """Whole-section transport and a fixed physical D-by-D central wake aperture."""
    values=[v[3] for v in p['samples'] if v[2]<.5]
    core=[]
    u,v=p['u_axis'],p['v_axis']
    bounds=p['world_bounds_m'];grid=p['grid']
    for k,cell in enumerate(p['samples']):
        i,j=k%p['width'],k//p['width']
        cu=bounds[u]+(i*grid[u]//p['width']+.5)*(bounds[u+3]-bounds[u])/grid[u]
        cv=bounds[v]+(j*grid[v]//p['height']+.5)*(bounds[v+3]-bounds[v])/grid[v]
        if cell[2]<.5 and abs(cu-(bounds[u]+bounds[u+3])/2)<=diameter/2 and abs(cv-(bounds[v]+bounds[v+3])/2)<=diameter/2:
            core.append(cell[3])
    return {'mean_vx_m_s':sum(values)/len(values) if values else None,
            'min_vx_m_s':min(values) if values else None,
            'reverse_flow_fraction':sum(v<0 for v in values)/len(values) if values else None,
            'normalized_mean_deficit':1-sum(values)/len(values)/speed if values else None,
            'scope':'unweighted samples across fluid cells in the entire YZ slice; not integrated drag',
            'central_wake':{'aperture_width_m':diameter,'sample_count':len(core),
                'mean_vx_m_s':sum(core)/len(core) if core else None,
                'normalized_mean_deficit':1-sum(core)/len(core)/speed if core else None,
                'scope':'D by D square at cross-section center, fluid samples only; no force interpretation'}}


def experiment(root, shape, fluid, speed, diameter, grid, dt, duration, iterations, legacy=False, length=2.0, sample_every=.2, window=1.0, criteria=None):
    root.mkdir(parents=True,exist_ok=False)
    s=Service(root/'session');run='case';revision=None
    try:
        scene=s.scene_create('shape',template='wind_empty' if shape=='empty' else 'wind_stl_'+shape,
                             inflow_speed=speed,object_size_m=diameter,dimensions=[length,1.0,1.0],
                             object_center_m=[.9,.5,.5])
        revision=scene['scene_revision']
        options=dict(grid=[grid,round(grid/length),round(grid/length)],fluid=fluid,qualification_mode=not legacy,solver_iterations=iterations)
        valid=s.scene_validate('shape',revision,dt=dt,**options)
        steps=round(duration/dt)
        if steps<1 or steps>=100000 or abs(steps*dt-duration)>1e-9:
            raise ValueError('duration must be an integer multiple of each dt and under 100000 steps')
        s.run_start(run,'shape',revision,steps=steps+1,dt=dt,**options)
        history=[]; wake_series=[]
        stride=max(1,round(sample_every/dt))
        positions={'upstream':.9-2*diameter,'near_wake':.9+diameter,'far_wake':.9+3*diameter}
        if min(positions.values())<0 or max(positions.values())>=length:
            raise ValueError('wake stations must lie inside the domain; reduce diameter or extend outlet')
        (root/'wake_samples').mkdir()

        for i in range(steps):
            await_receipt(s,run,revision,'step','step-'+str(i))
            snap=s.run_inspect(run)
            history.append({'tick':snap['tick'],'time_s':snap['simulation_time'],'health':snap['health']})
            if (i+1)%stride==0 or i+1==steps:
                observation={'tick':snap['tick'],'time_s':snap['simulation_time']}
                for name in ('near_wake','far_wake'):
                    envelope=sample(s,run,f'{name}-{i+1}',positions[name]/length)
                    if envelope['tick']!=snap['tick']: raise RuntimeError('incoherent wake sample tick')
                    profile=wake_profile(envelope['preview'],speed,diameter)
                    observation[name]={'central_mean_vx_m_s':profile['central_wake']['mean_vx_m_s'],
                        'section_mean_vx_m_s':profile['mean_vx_m_s'],
                        'reverse_flow_fraction':profile['reverse_flow_fraction'],
                        'slice_world_m':envelope['preview']['slice_world_m'],
                        'sample_count':profile['central_wake']['sample_count']}
                    atomic(root/'wake_samples'/f'{name}-{i+1}.json',envelope)
                wake_series.append(observation)
                atomic(root/'wake_series.json',wake_series)
        snap=s.run_inspect(run)
        planes={name:sample(s,run,name,x/length) for name,x in positions.items()}
        screen=steady_screen(wake_series,history,speed,length,dict(criteria or {},window_s=window))
        profiles={}
        for name,envelope in planes.items():
            p=envelope['preview']
            profiles[name]=wake_profile(p,speed,diameter)
        ref=reference(shape,diameter,speed,**dict(density=fluid['density_kg_m3'],viscosity=fluid['dynamic_viscosity_pa_s']))
        flags=['no_validated_force_integral','physical_outlet_not_qualified','collocated_projection_and_voxel_boundaries_unqualified']
        if screen['status']!='steady_window_screen_pass': flags.append('steady_state_not_established')
        if abs(profiles['upstream']['mean_vx_m_s']) < .01*speed:
            flags.append('driven_through_flow_not_established')
        balance=snap['health'].get('conservation',{})
        if not balance.get('available'): flags.append('final_conservation_unavailable')
        if snap['health'].get('projection_unconverged_regions',0): flags.append('pressure_not_converged')
        if legacy: flags.append('synthetic_wind_forcing_enabled')
        if diameter/valid['voxel_size_m']<8: flags.append('object_under_8_cells_across')
        if speed*duration/length<3: flags.append('under_3_domain_flow_through_times')
        if any(h['health']['velocity_clamped_cells'] for h in history):flags.append('velocity_safety_clamp_active')
        if any(h['health']['skipped_clusters'] for h in history):flags.append('solver_regions_skipped')
        result={'schema':'physics_sim_qualification_case_v1','shape':shape,'fluid':fluid,'dt_s':dt,
                'duration_s':duration,'steady_screen':screen,'wake_series':wake_series,
                'study_setup':{'dimensions_m':[length,1,1],'object_center_m':[.9,.5,.5],
                    'speed_m_s':speed,'wake_positions_m':positions,'sampling_interval_s':stride*dt},
                'grid_requested':options['grid'],'validation':valid,
                'scene_revision':revision,'snapshot':snap,'history':history,'wake_profiles':profiles,
                'reference':ref,'blockage_ratio':0 if shape=='empty' else ref['reference_area_m2'],
                'diameter_cells':diameter/valid['voxel_size_m'],'flow_through_times':speed*duration/length,
                'qualification_status':'not_qualified','reasons':flags}
        for name,p in planes.items():
            p['preview']['color_range']=[-speed,speed]
            image=image_content(p)
            if image:(root/(name+'.png')).write_bytes(base64.b64decode(image['data']))
            atomic(root/(name+'.json'),p)
        request=json.loads((s.run_dir(run)/'request.json').read_text())
        result['request']=request
        atomic(root/'report.json',result)
        return result
    finally:
        if revision and (s.root/'runs'/run/'request.json').exists():
            snap=s.run_inspect(run)
            if snap['state'] not in {'completed','cancelled','failed'}:
                await_receipt(s,run,revision,'cancel','qualification-finish')
        for child in s.children.values():child.wait(timeout=10)


def format_value(value):
    return f'{value:.6g}' if value is not None and math.isfinite(value) else 'unavailable'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True,help='New experiment directory; never overwrites an existing report')
    p.add_argument('--shapes',default='sphere,cube,cone')
    p.add_argument('--fluid',choices=FLUIDS,default='air_300k')
    p.add_argument('--density',type=float);p.add_argument('--viscosity',type=float)
    p.add_argument('--speed',type=float,default=.1);p.add_argument('--diameter',type=float,default=.25)
    p.add_argument('--grids',default='32');p.add_argument('--dts',default='.02');p.add_argument('--iterations',default='24')
    p.add_argument('--duration',type=float,default=.2)
    p.add_argument('--domain-length',type=float,default=2.0)
    p.add_argument('--sample-every',type=float,default=.2)
    p.add_argument('--steady-window',type=float,default=1.0)
    p.add_argument('--mean-drift-tolerance',type=float,default=.01)
    p.add_argument('--wake-range-tolerance',type=float,default=.02)
    p.add_argument('--flux-tolerance',type=float,default=1e-4)
    p.add_argument('--warmup-flow-throughs',type=float,default=3)
    p.add_argument('--legacy-wind',action='store_true',help='Measure synthetic legacy behavior for comparison, still using SI viscosity')
    p.add_argument('--compare',type=Path,help='Previous summary.json; report matched-case changes')
    a=p.parse_args();shapes=a.shapes.split(',')
    if not all(s in ('sphere','cube','cone','empty') for s in shapes):p.error('unknown shape')
    grids=[int(x) for x in a.grids.split(',')];dts=[float(x) for x in a.dts.split(',')];iters=[int(x) for x in a.iterations.split(',')]
    if len(shapes)*len(grids)*len(dts)*len(iters)>64:p.error('at most 64 cases per sweep')
    for dt in dts:
        if not math.isfinite(dt) or dt<.00001 or dt>.1:p.error('dt outside [.00001,.1]')
    if not math.isfinite(a.duration) or a.duration<=0:p.error('duration must be positive and finite')
    if not math.isfinite(a.domain_length) or not 2<=a.domain_length<=8: p.error('domain length must be in [2,8] meters')
    if not math.isfinite(a.sample_every) or a.sample_every<=0 or a.duration/a.sample_every>2048: p.error('positive sampling interval and at most 2048 observations required')
    if not math.isfinite(a.steady_window) or a.steady_window<=0: p.error('positive steady window required')
    criteria={'mean_drift_over_inlet_speed':a.mean_drift_tolerance,
              'range_over_inlet_speed':a.wake_range_tolerance,
              'relative_flux_imbalance':a.flux_tolerance,
              'min_flow_through_times':a.warmup_flow_throughs}
    if any(not math.isfinite(v) or v<=0 for v in criteria.values()): p.error('criteria tolerances must be positive and finite')
    fluid={'density_kg_m3':a.density if a.density is not None else FLUIDS[a.fluid]['density_kg_m3'],
           'dynamic_viscosity_pa_s':a.viscosity if a.viscosity is not None else FLUIDS[a.fluid]['dynamic_viscosity_pa_s']}
    a.output.mkdir(parents=True,exist_ok=False)
    reports=[]
    for shape in shapes:
        for grid in grids:
            for dt in dts:
                for it in iters:
                    name=f'{shape}-g{grid}-dt{dt:g}-i{it}'
                    print(name,flush=True)
                    report=experiment(a.output/name,shape,fluid,a.speed,a.diameter,grid,dt,a.duration,it,a.legacy_wind,a.domain_length,a.sample_every,a.steady_window,criteria)
                    report['fluid_reference']={'preset':a.fluid,**FLUIDS[a.fluid],
                        'overridden':a.density is not None or a.viscosity is not None}
                    atomic(a.output/name/'report.json',report)
                    reports.append({'case':name,'report':str((a.output/name/'report.json').resolve()),
                                    'physics_key':{'shape':shape,'fluid':fluid,'speed':a.speed,'diameter':a.diameter,'duration':a.duration,'domain_length':a.domain_length,'legacy':a.legacy_wind,'geometry_assets':report['request']['geometry_assets']},
                                    'wake_profiles':report['wake_profiles'],'health':report['snapshot']['health'],
                                    'steady_screen':report['steady_screen'],'reference':report['reference'],'qualification_status':report['qualification_status']})
                    atomic(a.output/'summary.json',{'schema':'physics_sim_qualification_sweep_v1','cases':reports})
    if a.compare:
        old={x['case']:x for x in json.loads(a.compare.read_text())['cases']}
        comparisons=[]
        for case in reports:
            prev=old.get(case['case'])
            if prev and prev['physics_key']==case['physics_key']:
                comparisons.append({'case':case['case'],'near_wake_mean_vx_change_m_s':
                    case['wake_profiles']['near_wake']['mean_vx_m_s']-prev['wake_profiles']['near_wake']['mean_vx_m_s'],
                    'poisson_residual_change_s_inv':case['health']['pressure_residual_linf_s_inv']-prev['health']['pressure_residual_linf_s_inv']})
        atomic(a.output/'comparison.json',{'previous':str(a.compare.resolve()),'matched_cases':comparisons})
    lines=['# Fluid qualification sweep', '',
           'These are solver diagnostics, not validated drag predictions. Reference forces are never applied to the solver.', '',
           '| Case | Reynolds | Central wake vx (m/s) | Poisson residual (1/s) | Steady screen | Physical status |',
           '|---|---:|---:|---:|---|---|']
    for c in reports:
        lines.append(f"| {c['case']} | {c['reference']['reynolds_number']:.6g} | {format_value(c['wake_profiles']['near_wake']['central_wake']['mean_vx_m_s'])} | {c['health']['pressure_residual_linf_s_inv']:.6g} | {c['steady_screen']['status']} | not qualified |")
    (a.output/'report.md').write_text('\n'.join(lines)+'\n')
    print(a.output/'summary.json')

if __name__=='__main__':main()
