"""Versioned app-owned numerical checks, never a physical accuracy certificate."""
import math


def finite(value):
    return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value)


def assess(snapshot, comparisons=None):
    gates={}
    def gate(name,value,limit):
        gates[name]={'status':'not_established' if not finite(value) else 'passed' if value<=limit else 'failed',
                     'value':value if finite(value) else None,'limit':limit}
    h=snapshot.get('health',{});p=snapshot.get('physics',{});f=snapshot.get('boundary_force_budget',{})
    rho=p.get('density_kg_m3');flux=h.get('volume_flux_m3_s');speed=p.get('mean_inlet_speed_m_s');dims=p.get('dimensions_m',[])
    mass=h.get('mass_balance_residual_kg_s');div=h.get('max_divergence_s_inv')
    scale=abs(rho*flux) if finite(rho) and finite(flux) else 0
    gate('relative_mass_imbalance',abs(mass)/scale if finite(mass) and scale>0 else None,1e-6)
    gate('relative_divergence',abs(div)*dims[1]/speed if finite(div) and finite(speed) and speed>0 and len(dims)==3 else None,1e-6)
    gates['projection']={'status':'passed' if h.get('projection_status')=='converged' else 'failed' if h.get('projection_status')=='failed' else 'not_established'}
    gates['steady_velocity']=snapshot.get('steady_acceptance',{'status':'not_established'})
    energy=snapshot.get('energy_budget',{})
    gate('energy_balance',energy.get('relative_residual') if energy.get('available') else None,.02)
    if p.get('stationary_obstacle') or f.get('available'):
        surface=f.get('surface_total_force_x_n');cv=f.get('control_volume_force_x_n')
        gate('surface_control_volume_force',abs(surface-cv)/max(abs(surface),abs(cv)) if finite(surface) and finite(cv) and max(abs(surface),abs(cv))>0 else None,.02)
    for kind in ('spatial','temporal'):
        comparison=(comparisons or {}).get(kind)
        if not comparison:
            gates[kind+'_refinement']={'status':'not_established','reason':'Provide a compatible completed coarser run.'}
            continue
        changes=comparison['comparisons'][-1]['changes']
        keys=['kinetic_energy_j','volume_flux_m3_s']+(['surface_total_force_x_n','control_volume_force_x_n'] if p.get('stationary_obstacle') or f.get('available') else [])
        values=[changes.get(k,{}).get('relative_to_fine') for k in keys]
        gates[kind+'_refinement']={'status':'not_established' if not all(finite(x) for x in values) else 'passed' if max(values)<=.02 else 'failed','limit':.02,'metrics':keys,'relative_changes':values}
    statuses=[x.get('status') for x in gates.values()]
    terminal_bad=snapshot.get('state') in ('failed','cancelled')
    status='failed' if terminal_bad or 'failed' in statuses or 'invalid' in statuses else 'passed' if all(x=='passed' for x in statuses) else 'not_established'
    return {'schema':'physics_sim_run_acceptance_v1','status':status,'run_id':snapshot.get('run_id'),
            'snapshot_sequence':snapshot.get('sequence'),'simulation_time':snapshot.get('simulation_time'),'gates':gates,
            'physical_accuracy_certified':False,'scope':'Numerical readiness only; reference accuracy, component forces, general obstacle energy, arbitrary geometry and 3D require separate evidence.'}
