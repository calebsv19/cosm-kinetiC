"""Separately bound long128 work capacity; physical v1 bundle stays fixed."""
from surface_sources.growth_fire_v1 import require
from . import scalar_wake128 as legacy
from .scalar_work_budget import require_horizon_work

PROFILE = 'scalar-wake128/v2'
MAX_CYCLES = 4
MAX_STEPS = legacy.MAX_STEPS
WORK_LIMIT = MAX_STEPS * 128**3 * 2 * MAX_CYCLES
Source = legacy.Source


def selected(request):
    return request.get('capacity_profile') == PROFILE


def validate(request):
    require(selected(request), 'closed128 work profile')
    legacy.validate(dict(request, capacity_profile=legacy.PROFILE))
    require(request['resource_limits']['scalar_work_cells'] == WORK_LIMIT,
            'explicit128 full-horizon work allowance')
    require_horizon_work(request, MAX_STEPS)


def capacity():
    return dict(profile=PROFILE, steps=MAX_STEPS, cells=128**3, scalar_stages=2,
                max_cycles_per_step=MAX_CYCLES, cumulative_work_limit=WORK_LIMIT,
                guarantee='Cumulative work fits if each accepted step uses at most four cycles; physical guards remain independent.')
