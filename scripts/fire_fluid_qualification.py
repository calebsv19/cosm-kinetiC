#!/usr/bin/env python3
"""Return-height/window sensitivity and stronger/longer domain qualification."""
import argparse,json,os
from pathlib import Path
from fire_fluid_feedback import configuration
from fire_fluid_regression import qualify
from coupling.qualification import matrix,comparison
from surface_sources.growth_fire_v1 import require


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output/'native-evidence')
    base=configuration(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker)
    rows={}
    for label,cfg in matrix(base):
        print('Qualifying '+label,flush=True)
        rows[label]=qualify(cfg,a.output/label)
        cp=json.loads((a.output/label/'accepted-checkpoint.json').read_text())
        held=cp['evidence']['held_return'];velocities=held['wind_input']['tangent_velocity_m_s']
        rows[label]['max_return_speed_m_s']=max((u*u+v*v)**.5 for u,v in velocities)
        rows[label]['domain_length_m']=cfg['fluid']['length_m']
        print(json.dumps({'case':label,'energy_j':rows[label]['metrics']['energy_j'],'time_s':rows[label]['metrics']['time_s']}),flush=True)
    pairs={label:comparison(rows['reference'],rows[label]) for label in ('height-half','height-double','exchange-half','exchange-slow','strong-wind')}
    pairs['domain-height']=comparison(rows['long-strong'],rows['taller-long-strong'])
    result={'schema':'fire_fluid_sensitivity_qualification/v1','status':'numerical_cases_passed',
        'cases':rows,'comparisons':pairs,'acceptance':'native_admission_conservation_recovery_and_export',
        'sensitivity_accuracy_qualified':False,'physical_calibration_qualified':False,
        'notes':['Case differences are diagnostics; no physical or mesh-accuracy tolerance is inferred from conservation.',
            'Longer cases end at one and two model seconds; no longer horizon is qualified.']}
    (a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'status':result['status'],'comparisons':pairs},indent=2),flush=True)
if __name__=='__main__':main()
