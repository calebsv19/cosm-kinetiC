"""Bind a joint source receiver to the native gas coordinate system."""
from surface_sources.growth_fire_v1 import areas, require
from surface_sources.receiver import mapping, policy_valid
from . import solid_box


def validate(config):
    receiver = config['receiver']
    fluid = config['fluid']
    policy = dict(schema='physics_sim_surface_source_policy/v1',
                  consumer_id='joint-geometry', model_id='native-gas-grid',
                  source_config=config['source'], stream_start_tick=0,
                  producer=dict(worker_sha256='0' * 64, adapter_sha256='0' * 64),
                  receiver=receiver)
    policy_valid(policy)
    for axis, name in enumerate(('X', 'Y', 'Z')):
        expected = dict(origin_m=0, dimensions=fluid['grid'][axis],
                        spacing_m=fluid['length_m'][axis] / fluid['grid'][axis])
        for field, native in expected.items():
            actual = receiver[field][axis]
            require(actual == native,
                    f'receiver {name} {field}={actual!r}; native gas requires {native!r}')
    if 'rig_geometry' in fluid:
        from .rig_geometry import topology
        mask = topology(fluid)
    else:
        mask = solid_box.topology(fluid)
    # Mapping needs only source geometry and dual areas, so admission can check
    # coverage and solid overlap before asking Fire to evolve a source frame.
    rows = mapping(dict(config=config['source'], surface=dict(area_m2=areas(config['source']))), policy)
    for row in rows:
        for cell, _ in row:
            require(mask[cell] == 0,
                    f'receiver source support cell {cell} overlaps native gas solid')
