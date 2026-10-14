#!/usr/bin/env python3
"""Repeatable opt-in prior-accepted held wind qualification, including disabled control."""
import argparse,copy,json,os
from pathlib import Path
from coupling.framework import Framework
from coupling.scenario import baseline
from fire_fluid_spatial import ground_configuration
from fire_fluid_regression import qualify
from surface_sources.growth_fire_v1 import require


def configuration(growth,fire_worker,fluid_worker,clock_worker,*,enabled=True,fire_denominator=50,fluid_denominator=200):
    c=ground_configuration(baseline(growth,fire_worker,fluid_worker,clock_worker,
                                    fire_denominator=fire_denominator,fluid_denominator=fluid_denominator))
    if enabled:
        c['source']['evolution']['model']='physical_rates_heat_advection_v1'
        c['return_channel']={'schema':'fire_fluid_prior_velocity_hold/v1','sample_height_m':.0625}
    return c


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output/'native-evidence')
    options=[('enabled',{}),('disabled',{'enabled':False}),('time-half',{'fire_denominator':100,'fluid_denominator':400})]
    summaries={}
    for label,extra in options:
        c=configuration(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker,**extra)
        summaries[label]=qualify(c,a.output/label)
    difference=summaries['enabled']['metrics']['energy_j']-summaries['disabled']['metrics']['energy_j']
    require(abs(difference)>1e-8,'coupled wind response did not change integrated source')
    enabled_cp=json.loads((a.output/'enabled/accepted-checkpoint.json').read_text())
    disabled_cp=json.loads((a.output/'disabled/accepted-checkpoint.json').read_text())
    require(enabled_cp['source']['checkpoint']['native_state_b64']!=disabled_cp['source']['checkpoint']['native_state_b64'],'feedback must affect native Fire state')
    metrics={name:row['metrics'] for name,row in summaries.items()}
    result={'schema':'fire_fluid_held_return_qualification/v1','status':'passed','cases':summaries,
        'enabled_minus_disabled_energy_j':difference,
        'half_dt_energy_relative_change':abs(metrics['time-half']['energy_j']/metrics['enabled']['energy_j']-1),
        'response_profile':'physical_rates_heat_advection_v1','return_channel':'fire_fluid_prior_velocity_hold/v1',
        'physical_calibration_qualified':False,'production_activation':False}
    (a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
