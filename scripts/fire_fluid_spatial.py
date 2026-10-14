#!/usr/bin/env python3
"""One immutable Fire timeline, three matched Fluid meshes, native open-floor sampling."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
from coupling.framework import Framework
from coupling.scenario import baseline
from coupling.allocation import allocate_window
from coupling.convergence import matched_comparisons
from coupling.velocity import observation
from evolving_atmosphere import run,SCHEMA
from open_atmosphere import run as open_run,SCHEMA as OPEN_SCHEMA
from surface_sources.growth_fire_v1 import digest,sealed,require


def write(path,value):path.write_text(json.dumps(value,allow_nan=False,indent=2)+'\n')


def ground_configuration(config):
    cfg=json.loads(json.dumps(config));f=cfg['fluid'];n=math.prod(f['grid']);plane=f['grid'][0]*f['grid'][1]
    # Authored divergence-free horizontal shear satisfies the floor exactly.
    h=f['length_m'][2]/f['grid'][2]
    f['initial_face_velocity_m_s']=[speed*((k+.5)*h) for speed in (.4,.1) for k in range(f['grid'][2]) for _ in range(plane)]+[0.]*(n+plane)
    f.update(initial_energy_j=[0.]*n,initial_smoke_kg=[0.]*n,
        boundary_policy={'schema':'physics_sim_open_reservoir_boundary/v1','horizontal':'periodic_xy',
            'vertical':'solid_bottom_open_top','predictor_velocity':'no_slip_bottom_zero_gradient_top',
            'pressure_datum_pa':[0,0],'inflow_temperature_k':[300,300],
            'inflow_smoke_concentration_kg_m3':[0,0],'scalar_diffusion':'zero_normal_flux'},
        buoyancy={'enabled':True,'gravity_m_s2':9.81,'expansion_per_k':1/300,'max_temperature_contrast_fraction':.1})
    return cfg


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','open-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--reuse-spatial',type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output/'native-evidence')
    base=baseline(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker,nodes=17)
    host=Framework(base,a.output/'unused-store')
    plan=host.clock(dict(host.base_plan,initial_id=1,initial_validation=1,initial_checkpoints=[1,2],history=[]))
    source=host.fire.initial(base['source'],a.fire_worker,'matched-initial');frames=[]
    for window in plan['schedule']:
        source=host.fire.advance_exact(source,a.fire_worker,window['fire_steps'],'matched-'+str(len(frames)),base['periods'][0]);frames.append(source['frame'])
    write(a.output/'source-frames.json',frames)
    if a.reuse_spatial:
        require(json.loads((a.reuse_spatial/'source-frames.json').read_text())==frames,'reused source timeline identity')
    fields=[];cases=[]
    for grid in (8,16,32):
        cfg=baseline(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker,nodes=17,grid=grid)
        if a.reuse_spatial:
            from numeric_digest_stream import digest as native_digest
            path=(a.reuse_spatial/f'grid-{grid}.json').resolve();result=json.loads(path.read_text())
            require(result['digest']==native_digest(result),'reused native result digest')
            Framework(cfg,a.output/'unused-store').native_checkpoint(source,result)
            write(a.output/f'grid-{grid}-reference.json',{'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'result_digest':result['digest']})
        else:
            state=None;result=None
            for frame,window in zip(frames,plan['schedule']):
                policy={'schema':'physics_sim_surface_source_policy/v1','consumer_id':'matched-fluid','model_id':'exact-window-uniform-source',
                    'source_config':cfg['source'],'producer':source['checkpoint']['producer'],'stream_start_tick':0,'receiver':cfg['receiver']}
                result=run(dict(cfg['fluid'],schema=SCHEMA,state=state,steps=allocate_window(frame,policy,window)),a.fluid_worker);state=result['state']
            host.unchanged();write(a.output/f'grid-{grid}.json',result)
        fields.append(result['fields']);cases.append({'grid':cfg['fluid']['grid'],'digest':result['digest'],
            'peak_temperature_k':max(result['fields']['temperature_k']),'budgets':result['budgets']})
    pairs,native_pairs=matched_comparisons(fields,[case['grid'] for case in cases])
    ratios={name:pairs[1][name]['relative_l1']/pairs[0][name]['relative_l1'] for name in pairs[0]}
    write(a.output/'spatial-comparison.json',{'schema':'fire_fluid_matched_spatial_comparison/v1',
        'source_frames_digest':hashlib.sha256((a.output/'source-frames.json').read_bytes()).hexdigest(),
        'configuration':base,'fixed_source_nodes':17,'fixed_injection_depth_m':.125,'fixed_dt_s':.005,'fixed_duration_s':.2,
        'method':'conservative volume restriction onto fixed 8x8x4 observation partition','cases':cases,'pairs':pairs,
        'pair_native_partition_diagnostics':native_pairs,
        'fine_to_coarse_error_ratio':ratios,'errors_decrease':all(v<1 for v in ratios.values()),
        'physical_calibration_qualified':False,'asymptotic_order_qualified':False})
    cfg=ground_configuration(base);cfg['fluid_worker']=str(a.open_worker.resolve())
    write(a.output/'ground-config.json',cfg)
    ground=Framework(cfg,a.output/'ground-joint');ground.initialize()
    for revision,frame in enumerate(frames):
        ground.step(revision,'ground-'+str(revision))
        require(ground.inspect()['source']['frame']['cells']==frame['cells'],'matched ground source timeline')
    cp=ground.inspect();result=cp['fluid']
    write(a.output/'ground-joint-checkpoint.json',cp)
    wind=observation(cp,cfg,plan['schedule'][-1]['end'],0,height_m=.0625)
    write(a.output/'ground-result.json',result);write(a.output/'ground-observation.json',wind)
    write(a.output/'summary.json',{'spatial_errors_decrease':all(v<1 for v in ratios.values()),'error_ratios':ratios,
        'ground_observation_digest':wind['digest'],'ground_model':result['model'],'ground_budgets':result['budgets'],
        'sample_height_m':.0625,'joint_checkpoint_digest':cp['digest'],'joint_revision':cp['revision'],'applied_to_fire':False})
    print((a.output/'summary.json').read_text())
if __name__=='__main__':main()
