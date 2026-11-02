"""Fixed diagnostic recipes for optional live inert-object checkpoint proofs."""
import copy
from .wind_presets import configuration
from .wind_sensitivity import refined
from .open_boundaries import layout


def object_config(h=2):
    return {'schema':'physics_sim_inert_object/v1','axis':0,'side':-1,
        'initial_temperature_k':300.,'capacity_j_k':10.,'h_w_m2_k':h,'support_depth_m':.125}


def recipe(workers,*,live=False,h=2):
    c=refined(configuration(workers,'channel_full',duration=1.2,exchange=.1),16)
    f=c['fluid'];f['buoyancy']['enabled']=live
    f['properties']['tracer_diffusivity_m2_s']=0.
    f['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
    f['solid_box']={'schema':'physics_sim_stationary_box/v1',
        'lower_m':[.375,.375,.0625 if live else .375],
        'upper_m':[.625,.625,.3125 if live else .625],
        'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    f['inert_object']=object_config(h)
    f['resource_limits']={'max_cells':32768,'scalar_work_cells':1000000000,
        'numerical_bytes':512*1024**2,'cache_pressure_operator':True}
    c['journal_codec']='lossless_f64_zlib/v1'
    if live:
        s=c['source'];s.update(run_id='inert-live-reference',origin_m=[.0625,.4375,0],cell_size_m=.125/16)
        s['evolution']['ignition_rectangle_m']=[.03125,.03125,.09375,.09375]
    return c


def heater_request(workers,*,steps=240,h=2):
    from open_atmosphere import SCHEMA
    f=recipe(workers,h=h)['fluid'];n=16**3;source=[0.]*n;zero=[0.]*n
    for k in range(6,10):
        for j in range(6,10):source[(k*16+j)*16+2]=.5/16
    return dict(f,schema=SCHEMA,state=None,
        steps=[{'energy_j':source if i<80 else zero,'smoke_kg':zero} for i in range(steps)])
