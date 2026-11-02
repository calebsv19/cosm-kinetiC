"""Bounded channel/cadence comparisons using existing atomic live coupling."""
from .wind_presets import configuration
from .wind_sensitivity import refined
from surface_sources.growth_fire_v1 import require


def configuration_for(workers, name):
    require(name in ('cadence-100ms','cadence-40ms','vent-full',
                     'vent-elevated','vent-obstacle'), 'campaign case')
    cadence = name.startswith('cadence-')
    exchange = .04 if name == 'cadence-40ms' else .1
    profile = 'abrupt_gust' if cadence else {
        'vent-full':'channel_full','vent-elevated':'channel_patch',
        'vent-obstacle':'channel_patch_box'}[name]
    c = refined(configuration(workers, profile, duration=1.2, exchange=exchange,
                              gust_start=.2, gust_stop=.6), 64 if cadence else 32)
    f = c['fluid'];f['momentum_dt_s'] = .0025;c['periods'][1] = [1,400]
    for knot in f['wind_forcing']['knots']:knot['step'] *= 2
    f['wind_forcing']['momentum_backflow'] = 'quiescent_exterior/v1'
    if cadence:
        f['solid_box'] = {'schema':'physics_sim_stationary_box/v1',
            'lower_m':[.375]*3,'upper_m':[.625]*3,
            'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    f['resource_limits'] = {'max_cells':max(32768,c['fluid']['grid'][0]**3),
        'scalar_work_cells':1000000000,'numerical_bytes':512*1024**2,
        'cache_pressure_operator':True,'native_request_bytes':256*1024**2,
        'native_output_bytes':256*1024**2}
    c['journal_codec'] = 'lossless_f64_zlib/v1'
    c['native_packet_steps'] = 8 if exchange == .04 else 10
    return c
