"""Explicit fixed-MAC diagnostic admission; never a live momentum solution."""
from surface_sources.growth_fire_v1 import require

MODE = 'fixed_mac_velocity/v1'


def validate(config, data=None):
    require('open_domain_forcing' not in config,'fixed MAC excludes momentum driving')
    require(config.get('diagnostic_transport') == MODE, 'fixed MAC diagnostic schema')
    require(config['buoyancy']['enabled'] is False and 'solid_box' in config,
            'fixed MAC diagnostic requires inert box and disabled buoyancy')
    boundary = config['boundary_policy']
    require(boundary['horizontal'] == 'ambient_open_xy' and
            boundary['vertical'] == 'solid_bottom_open_top', 'fixed MAC boundary envelope')
    wind = config['wind_forcing']
    require(wind.get('momentum_backflow') == 'quiescent_exterior/v1' and
            len(wind['knots']) == 1, 'fixed MAC constant boundary and kinetic ledger')
    if data is not None:
        require(data['face_velocity_m_s'] == config['initial_face_velocity_m_s'],
                'fixed MAC field changed')
