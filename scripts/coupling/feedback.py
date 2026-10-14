"""Accepted prior Fluid observation held over one Fire window; no staged lookahead."""
import base64
import struct
from .velocity import observation
from surface_sources.growth_fire_v1 import keys,require,number,sealed


def validate(config):
    profile=config['source'].get('evolution',{}).get('model') in ('physical_rates_heat_advection_v1','physical_rates_heat_outflow_v1')
    enabled='return_channel' in config
    require(profile==enabled,'wind profile requires explicit held return channel')
    if not enabled:return
    channel=config['return_channel'];keys(channel,('schema','sample_height_m'))
    require(channel['schema']=='fire_fluid_prior_velocity_hold/v1','return channel version')
    number(channel['sample_height_m'],0,config['fluid']['length_m'][2])
    require(0<channel['sample_height_m']<config['fluid']['length_m'][2],'interior return height')
    boundary=config['fluid'].get('boundary_policy',{})
    require(boundary.get('vertical')=='solid_bottom_open_top' and boundary.get('predictor_velocity')=='no_slip_bottom_zero_gradient_top','qualified return boundary model')


def sample(parent,config,window,fire):
    value=observation(parent,config,window['begin'],window['end']-window['begin'],
                      height_m=config['return_channel']['sample_height_m'])
    require(value['checkpoint_tick']==window['begin'],'accepted prior return endpoint')
    from fire_source.wind import payload
    wind=payload(value['tangent_velocity_m_s'])
    # Admission before any solver evolution, including m/s bounds and f32 conversion.
    raw=base64.b64decode(parent['source']['checkpoint']['native_state_b64'])
    from fire_source.wind import inject
    _,wind_digest=inject(raw,wind)
    return sealed({'schema':'fire_fluid_held_wind/v1','observation':value,'wind_input':wind,
        'wind_input_digest':wind_digest,'held_window_ticks':[window['begin'],window['end']],
        'consumer':config['source']['evolution']['model'],'applied_to_fire':True})


def consumed(source,held,fire):
    require(source['operation']['wind_digest']==held['wind_input_digest'],'Fire wind consumption identity')
    state=fire.state_decode(base64.b64decode(source['checkpoint']['native_state_b64']))
    vectors=held['wind_input']['tangent_velocity_m_s']
    for axis,key in enumerate(('wind_x','wind_y')):
        expected=[struct.unpack('<f',struct.pack('<f',v[axis]))[0] for v in vectors]
        require(state['fields'][key]==expected,'persisted held wind consumption')
