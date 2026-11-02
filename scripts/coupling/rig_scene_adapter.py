"""Scoped carrier extension for a validated rig; preserves the real receiving model.

The frozen Growth carrier owns VF3D and scene packaging. This app registers its
new model/mask only while exporting or validating that model, then restores the
carrier globals even on failure. Solver states and Growth source are unchanged.
"""
from contextlib import contextmanager
from . import rig_geometry
from surface_sources.growth_fire_v1 import require


@contextmanager
def receiving_model(carrier,result):
    if result['model']!=rig_geometry.MODEL:
        yield;return
    previous_mask=carrier.mask_bytes
    require(rig_geometry.MODEL not in carrier.MODELS,'rig carrier already registered')
    def mask(value):
        if value['model']!=rig_geometry.MODEL:return previous_mask(value)
        config=value['configuration'];expected=rig_geometry.topology(config)
        require(value['fields']['solid_mask']==expected,'rig carrier/native geometry identity')
        rig_geometry.check(config,value['state']['data'],expected)
        return bytes(expected)
    carrier.MODELS[rig_geometry.MODEL]=('physics_sim_open_atmosphere_experiment/v1','physics_sim_open_coupled_checkpoint/v1')
    carrier.mask_bytes=mask
    try:yield
    finally:
        carrier.mask_bytes=previous_mask
        del carrier.MODELS[rig_geometry.MODEL]
