"""Closed opt-in scalar reconstruction/integration identity; defaults stay absent."""
from surface_sources.growth_fire_v1 import require
FIELD = 'scalar_transport'
PROFILE = 'mc_ssprk2/v1'


def validate(config):
    if FIELD not in config:
        return
    require(type(config[FIELD]) is str and config[FIELD] == PROFILE,
            'supported opt-in scalar profile')
    if 'diagnostic_transport' in config:
        require(config['diagnostic_transport'] == 'fixed_mac_velocity/v1',
                'scalar profile requires the explicit fixed MAC diagnostic')


def state(config, data):
    validate(config)
    if FIELD in config:
        require(data.get(FIELD) == config[FIELD], 'native scalar profile identity')
    else:
        require(FIELD not in data, 'unbound scalar profile receipt')


def fields(config, output, data):
    state(config, data)
    if FIELD in config:
        require(output.get(FIELD) == data[FIELD], 'scalar field/state profile identity')
    else:
        require(FIELD not in output, 'unbound scalar field profile')
