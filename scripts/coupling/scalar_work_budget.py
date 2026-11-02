"""Necessary scalar horizon capacity, not a forecast of adaptive CFL cost.

The work counter is cumulative across packets and checkpoint restores. Each
step needs at least one scalar cycle; SSPRK2 charges two stages per cycle.
Passing this lower bound does not guarantee that later adaptive cycles fit.
"""
import math
from surface_sources.growth_fire_v1 import require


def require_horizon_work(config, end_step, *, first_step=0, used=0):
    require(type(end_step) is int and type(first_step) is int
            and 0 <= first_step <= end_step and type(used) is int and used >= 0,
            'scalar work horizon/counter')
    stages = 2 if config.get('scalar_transport') == 'mc_ssprk2/v1' else 1
    cells = math.prod(config['grid'])
    limit = config.get('resource_limits', {}).get('scalar_work_cells', 100000000)
    require(type(limit) is int and limit >= used, 'scalar work limit/counter')
    minimum = (end_step-first_step)*cells*stages
    require(minimum <= limit-used,
            f'scalar work horizon cannot fit: used={used} minimum_remaining={minimum} '
            f'limit={limit}; adaptive CFL cycles may require more')
    return dict(used=used, minimum_remaining=minimum, limit=limit,
                minimum_total=used+minimum, adaptive_cost_unproven=True)
