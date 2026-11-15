"""Versioned admission for the qualified stationary low-box wall closure.

The first admission deliberately covers the tested 4/8-layer geometry only.
No numerical implementation lives in this protocol bridge.
"""
from surface_sources.growth_fire_v1 import require
FIELD = 'viscous_wall_closure'
PROFILE = 'low_box_quadratic/v1'


def validate(config):
    if FIELD not in config:
        return
    require(type(config[FIELD]) is str and config[FIELD] == PROFILE,
            'supported opt-in viscous wall closure')
    require(config.get('grid') in ([32,32,32], [32,32,64]) and
            config.get('length_m') == [.5,.5,.5], 'qualified low-box wall grid/domain')
    b = config.get('solid_box', {})
    require(b.get('lower_m') == [.125,.125,.0625] and
            b.get('upper_m') == [.375,.375,.3125] and
            b.get('velocity_boundary') == 'no_slip' and
            b.get('thermal_boundary') == 'insulated', 'qualified low-box wall geometry')
    boundary = config.get('boundary_policy', {})
    require(boundary.get('horizontal') == 'ambient_open_xy' and
            boundary.get('vertical') == 'solid_bottom_open_top', 'qualified wall boundaries')
    require(not any(k in config for k in ('capacity_profile', 'diagnostic_transport',
            'inert_object', 'wind_forcing', 'rig_geometry')), 'separate wall admission modes')


def state(config, data):
    validate(config)
    if FIELD in config:
        require(data.get(FIELD) == config[FIELD], 'native viscous wall closure identity')
    else:
        require(FIELD not in data, 'unbound viscous wall closure receipt')


def fields(config, output, data):
    state(config, data)
    if FIELD in config:
        require(output.get(FIELD) == data[FIELD], 'wall field/state closure identity')
    else:
        require(FIELD not in output, 'unbound wall field closure')
