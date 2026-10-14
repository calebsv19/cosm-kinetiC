#!/usr/bin/env python3
"""Qualify the actual native adapters, joint publication, and one bounded scenario."""
import argparse
import json
import os
from pathlib import Path
import shutil
from coupling.framework import Framework
from coupling.scenario import baseline,metrics
from surface_sources.growth_fire_v1 import require


def qualify(config,output):
    output.mkdir(parents=True,exist_ok=False)
    (output/'config.json').write_text(json.dumps(config,indent=2)+'\n')
    host=Framework(config,output/'run');host.initialize();receipts=[]
    for revision in range(2):receipts.append(host.step(revision,'window-'+str(revision)))
    before=host.path.read_bytes()
    require(host.step(1,'window-1')==receipts[-1] and host.path.read_bytes()==before,'replay changed ledger')
    try:host.step(0,'stale')
    except ValueError:pass
    else:raise ValueError('stale predecessor accepted')
    require(host.path.read_bytes()==before,'stale operation mutated ledger')
    recovered=output/'recovered';recovered.mkdir();shutil.copy2(host.path,recovered/host.path.name)
    replay=Framework(config,recovered)
    def fail(cp):raise RuntimeError('injected failure after native staging, before publication')
    try:host.step(2,'window-2',before_publish=fail)
    except RuntimeError:pass
    else:raise ValueError('fault injection did not run')
    require(host.path.read_bytes()==before and host.inspect()['revision']==2,'failed staging changed accepted head')
    for revision in range(2,config['windows']):
        receipt=host.step(revision,'window-'+str(revision))
        require(replay.step(revision,'window-'+str(revision))==receipt,'restart receipt differs');receipts.append(receipt)
    cp=host.inspect();require(replay.inspect()==cp,'restart checkpoint differs')
    from coupling.authoring import export
    projection=export(host,output/'scene')
    from coupling.velocity import observation
    wind=observation(cp,config,cp['evidence']['window_ticks'][1],0,height_m=config.get('return_channel',{}).get('sample_height_m',config['receiver']['spacing_m'][2]/2))
    (output/'native-velocity-observation.json').write_text(json.dumps(wind,allow_nan=False)+'\n')
    summary={'status':'passed','windows':cp['revision'],'ticks_per_second':cp['evidence']['rate'],
        'fire_steps':sum(r['fire_steps'] for r in receipts),'fluid_steps':sum(r['fluid_steps'] for r in receipts),
        'exact_replay':True,'exact_restart':True,'stale_refusal':True,'prepublication_failure_rollback':True,
        'metrics':metrics(cp),'physical_calibration_qualified':False,'bidirectional_feedback':'return_channel' in config,
        'boundary_model':cp['fluid']['model'],'scene_projection':projection,
        'scene_vf3d_validation':True}
    if 'return_channel' in config:
        summary['held_return']={'schema':config['return_channel']['schema'],
            'sample_height_m':config['return_channel']['sample_height_m'],
            'prior_checkpoint_digest':cp['evidence']['held_return']['observation']['joint_checkpoint_digest'],
            'wind_input_digest':cp['evidence']['held_return']['wind_input_digest'],
            'applied_to_fire':True}
    summary['native_return_observation']={'digest':wind['digest'],'sample_height_m':wind['sample_height_m'],
        'first_tangent_velocity_m_s':wind['tangent_velocity_m_s'][0],'applied_to_fire':False}
    (output/'accepted-checkpoint.json').write_text(json.dumps(cp,allow_nan=False)+'\n')
    (output/'summary.json').write_text(json.dumps(summary,indent=2,allow_nan=False)+'\n')
    return summary


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--refinement',action='store_true');a=p.parse_args()
    config=baseline(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker)
    # Native execution retains bounded diagnostics outside disposable build roots.
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output.resolve()/'native-evidence')
    result=qualify(config,a.output/'baseline')
    if a.refinement:
        rows=[]
        for label,options in [('time-half',{'fire_denominator':100,'fluid_denominator':400}),
                              ('source-grid-half',{'nodes':17}),('fluid-grid-half',{'grid':16})]:
            candidate=baseline(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker,**options)
            proof=qualify(candidate,a.output/label)
            reference=result['metrics'];m=proof['metrics']
            rows.append({'case':label,'metrics':m,'energy_relative_change':abs(m['energy_j']/reference['energy_j']-1),
                'smoke_relative_change':abs(m['smoke_kg']/reference['smoke_kg']-1),
                'temperature_peak_change_k':m['temperature_peak_k']-reference['temperature_peak_k']})
        (a.output/'refinement.json').write_text(json.dumps({'status':'diagnostic','cases':rows,
            'physical_calibration_qualified':False,'acceptance':'conservation_and_recovery_only'},indent=2)+'\n')
        result['refinement']=rows
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
