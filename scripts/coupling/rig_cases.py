"""One fixed partition/window scene with paired forcing and unchanged native clocks."""
import copy
from .open_weather_cases import configuration as reference
from . import rig_geometry
from surface_sources.growth_fire_v1 import require


def configuration(workers,variant,*,grid=32,windows=120,window_open=True):
    require(variant in ('calm','gust') and grid in (16,32) and type(windows) is int and 1<=windows<=120,'rig case selection')
    c=reference(workers,'gust',grid);f=c['fluid']
    f.pop('solid_box');f.pop('inert_object')
    f['rig_geometry']=dict(schema=rig_geometry.SCHEMA,window_open=window_open)
    f['scalar_transport']='mc_ssprk2/v1';c['journal_codec']='lossless_f64_zlib/v1'
    c['windows']=windows;c.pop('native_packet_steps',None)
    if windows<=16:c.pop('source_delivery',None)
    else:c['source_delivery']['active_windows']=16
    if variant=='calm':
        for knot in f['open_domain_forcing']['knots']:knot['strength']=0
    mask=rig_geometry.topology(f);n=grid;depth=round(c['receiver']['injection_depth_m']*n)
    c['receiver']['fluid_mask']=[1-v for v in mask[:n*n]]
    c['receiver']['injection_fluid_mask']=[1-v for v in mask[:depth*n*n]]
    rig_geometry.components(f,mask)
    return c
