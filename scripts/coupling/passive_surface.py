"""Explicit, non-burning lumped boundary for prescribed gas temperatures.

This qualification model does not debit the native gas solver. Its transfer
ledger is a demand for a future reciprocal adapter, not an exchanged amount.
The constant-coefficient window has an exact exponential solution.
"""
import math


def advance(state, gas_k, dt, *, area_m2, capacity_j_k, h_w_m2_k,
            cooling_w_k=0., ambient_k=300.):
    values=(gas_k,dt,area_m2,capacity_j_k,h_w_m2_k,cooling_w_k,ambient_k,
            state['temperature_k'],state['time_s'],state['stored_j'])
    if not all(math.isfinite(v) for v in values) or min(gas_k,dt,area_m2,capacity_j_k,ambient_k,state['temperature_k'])<=0 or min(h_w_m2_k,cooling_w_k)<0:
        raise ValueError('finite positive SI boundary quantities required')
    conductance=area_m2*h_w_m2_k
    total=conductance+cooling_w_k
    old=state['temperature_k']
    if total:
        equilibrium=(conductance*gas_k+cooling_w_k*ambient_k)/total
        change=(equilibrium-old)*(-math.expm1(-total*dt/capacity_j_k))
        integral=equilibrium*dt+(old-equilibrium)*capacity_j_k/total*(-math.expm1(-total*dt/capacity_j_k))
    else:
        change=0.;integral=old*dt
    gas=conductance*(gas_k*dt-integral)
    cooling=cooling_w_k*(integral-ambient_k*dt)
    stored=capacity_j_k*change
    result=dict(temperature_k=old+change,time_s=state['time_s']+dt,stored_j=state['stored_j']+stored)
    return result,dict(prescribed_gas_transfer_j=gas,cooling_j=cooling,stored_change_j=stored,
                       residual_j=gas-cooling-stored,reciprocal_gas_debit_applied=False)


def require_burning_admission(checkpoint):
    required=('local_heat_converged','gas_surface_flux_converged',
              'surface_response_numerics_qualified','reciprocal_exchange_qualified',
              'material_ignition_qualified')
    missing=[name for name in required if checkpoint.get(name) is not True]
    if missing:
        raise ValueError('burning faces disabled: '+', '.join(missing))
