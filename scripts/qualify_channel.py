#!/usr/bin/env python3
"""Reproduce the bounded channel verification campaign through the agent service."""
import argparse
import json
import math
from pathlib import Path
import sys
import time
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent/'agent_session'))
from service import Service,TERMINAL
from protocol import call

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model',choices=['reduced','mac2d'],default='reduced')
    parser.add_argument('--output',type=Path,required=True,help='New immutable evidence directory')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    service=Service(args.output/'session');rows=[]
    mac=args.model=='mac2d'
    fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}
    for name,g,bottom,top in [('poiseuille',.1,0,0),('couette',0,-.2,.5),('combined',.1,-.2,.5),('reverse',-.1,0,0)]:
        scene=call(service,'scene_create',dict(scene_id=name,template='cfd_channel_2d' if mac else 'cfd_channel',dimensions=[2,1,.5],channel={'pressure_gradient_pa_m':g,'wall_bottom_m_s':bottom,'wall_top_m_s':top}))
        for n in (16,32,64):
            run=f'{name}-{n}'
            call(service,'run_start',dict(request_id=run,scene_id=name,scene_revision=scene['scene_revision'],grid=[4 if mac else 1,n,1],fluid=fluid,dt=.05,steps=800,start_paused=False))
            try:
                deadline=time.monotonic()+30
                while time.monotonic()<deadline:
                    snap=service.run_inspect(run)
                    if snap['state'] in TERMINAL:break
                    time.sleep(.01)
                if snap['state']!='completed':raise RuntimeError(f'{run}: {snap}')
                result=service.run_result(run);p=snap['physics'];h=snap['health']
                exact_flow=.5*((bottom+top)/2+g/(12*.1))
                error=max(abs(u-(bottom+(top-bottom)*y+g*y*(1-y)/(.2))) for y,u in p['x_mean_profile_y_m_vx_m_s' if mac else 'profile_y_m_vx_m_s'])
                pressure_check=-(p['wall_on_fluid_bottom_shear_pa']+p['wall_on_fluid_top_shear_pa'])*2 if mac else h['pressure_drop_from_momentum_pa']
                momentum=h['streamwise_momentum_balance_residual_n'] if mac else h['momentum_balance_residual_n']
                energy=None if mac else h['energy_balance_residual_w']
                acceleration=h['max_predictor_acceleration_m_s2'] if mac else h['max_acceleration_m_s2']
                shear_error=max(abs(p['wall_on_fluid_bottom_shear_pa']+(.1*(top-bottom)+g/2)),abs(p['wall_on_fluid_top_shear_pa']-(.1*(top-bottom)-g/2)))
                row={'case':name,'cells':n,'max_velocity_error_m_s':error,'volume_flux_m3_s':h['volume_flux_m3_s'],'reference_volume_flux_m3_s':exact_flow,
                     'wall_shear_error_pa':shear_error,'pressure_drop_pa':p['pressure_drop_pa'],'pressure_drop_check_pa':pressure_check,'pressure_check_scope':'steady wall-force balance' if mac else 'transient momentum balance',
                     'momentum_balance_residual_n':momentum,'energy_balance_residual_w':energy,
                     'max_acceleration_m_s2':acceleration,'worker_sha256':result['provenance']['worker_sha256'],
                     'result_path':str((service.run_dir(run)/'result.json').resolve())}
                scale=max(abs(bottom),abs(top),abs(g)/(.8),1e-12)
                row['passed']=error/scale<.005 and shear_error<1e-9 and abs(pressure_check-2*g)<1e-9 and abs(momentum)<1e-9 and (energy is None or abs(energy)<1e-9) and acceleration<1e-9 and (not mac or h['max_divergence_s_inv']<1e-9) and abs(h['volume_flux_m3_s']-exact_flow)<.0004
                rows.append(row)
            finally:
                if service.run_inspect(run)['state'] not in TERMINAL:service.run_control(run,'cleanup','cancel',scene['scene_revision'],5000)
    convergence={}
    for name in ('poiseuille','combined','reverse'):
        errors=[r['max_velocity_error_m_s'] for r in rows if r['case']==name]
        convergence[name]=[math.log(errors[i]/errors[i+1],2) for i in (0,1)]
    passed=all(r['passed'] for r in rows) and all(1.95<v<2.05 for orders in convergence.values() for v in orders)
    report={'schema':'physics_sim_channel_verification_v1','passed':passed,'model':args.model,'scope':'Laminar parallel-wall verification; MAC 2D when selected. No 3D or open-outlet acceptance',
            'observed_spatial_orders':convergence,'cases':rows}
    (args.output/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps({'passed':passed,'cases':len(rows),'spatial_orders':convergence,'report':str(args.output/'report.json')},indent=2))
    return 0 if passed else 1
if __name__=='__main__':raise SystemExit(main())
