"""Workbench policy for native inlet recipes; existing scene settings stay intact."""
from fractions import Fraction
from .wind_presets import PROFILES,configuration
from surface_sources.growth_fire_v1 import require,keys,number
LABELS={'continuous':'Sustained wind','abrupt_gust':'Abrupt gust','ramped_gust':'Ramped gust',
 'channel_full':'Channel — full inlet','channel_patch':'Channel — elevated inlet',
 'channel_patch_box':'Channel — elevated inlet + box','single_gust_recovery':'Single gust and recovery',
 'repeated_gust':'Repeated gusts','balanced_forward':'Paired boundaries — forward flow',
 'balanced_reversal':'Paired boundaries — wind reversal'}
DESCRIPTIONS={
 'continuous':'Prescribed left inlet, open side and top reservoirs, solid no-slip floor.',
 'abrupt_gust':'The inlet switches on and off; Fire receives the previously accepted gas velocity.',
 'ramped_gust':'The inlet rises, holds, and fades linearly on the native gas clock.',
 'channel_full':'Full left inlet and right pressure outlet. Side and ceiling walls are impermeable slip boundaries; floor is no-slip.',
 'channel_patch':'A 0.5 × 0.5 m opening occupies Y,Z = 0.25–0.75 m. Right pressure outlet; impermeable slip sides/ceiling and no-slip floor.',
 'channel_patch_box':'The elevated inlet faces a fixed 0.25 m insulated cube. Flow cannot cross the cube; there is no solid heat transfer.',
 'single_gust_recovery':'One finite gust followed by a longer recovery. Defaults: on 0.4–1.2 s, observe through 4.8 s.',
 'repeated_gust':'Two equal-duration gusts. The second starts 0.8 s after the first shuts off; defaults end at 2.8 s.',
 'balanced_forward':'Both X ends prescribe the same signed flow until 2.4 s; Y/top reservoirs stay open.',
 'balanced_reversal':'Both X ends prescribe forward flow to 1.2 s, reverse to 2.4 s, then zero. This does not switch an inlet/pressure-outlet pair.'}
WIND_KEYS=('feedback','source_scale','sample_height_m','exchange_s','duration_s',
           'inlet_speed_m_s','gust_start_s','gust_stop_s','ramp_s','profile')


def is_wind(profile):return profile in PROFILES


def defaults(profile):
    require(is_wind(profile),'native wind profile')
    long=profile in ('single_gust_recovery','repeated_gust','balanced_forward','balanced_reversal')
    return {'feedback':True,'source_scale':1.,'sample_height_m':.125,'exchange_s':.2 if long else .1,
            'duration_s':4.8 if long else 3.2,'inlet_speed_m_s':1.75 if profile in ('channel_patch','channel_patch_box') else .5,
            'gust_start_s':.4,'gust_stop_s':1.2,'ramp_s':.4,'profile':profile}


def metadata():return {p:{'label':LABELS[p],'description':DESCRIPTIONS[p],'defaults':defaults(p)} for p in PROFILES}


def valid_settings(s):
    keys(s,WIND_KEYS);require(is_wind(s['profile']),'native wind profile');require(type(s['feedback']) is bool,'feedback must be boolean')
    for key,lo,hi in (('source_scale',.1,5),('sample_height_m',.001,.499),('duration_s',.1,6.4),
                       ('inlet_speed_m_s',0,2),('gust_start_s',0,40),('gust_stop_s',0,40),('ramp_s',0,10)):
        number(s[key],lo,hi)
    require(s['exchange_s'] in (.02,.04,.1,.2),'exchange must align with native steps: 0.02, 0.04, 0.1 or 0.2 s')
    duration=Fraction(str(s['duration_s']));exchange=Fraction(str(s['exchange_s']));count=duration/exchange
    require(count.denominator==1 and 3<=count<=32,'duration must use 3–32 whole exchanges; choose a shorter duration or slower exchange')
    start=Fraction(str(s['gust_start_s']));stop=Fraction(str(s['gust_stop_s']));ramp=Fraction(str(s['ramp_s']))
    require(start<stop,'gust stop must follow gust start')
    for t in (start,stop,ramp):require((t/Fraction(1,200)).denominator==1,'wind timing must land on a 0.005 s gas step')
    if s['profile'] in ('abrupt_gust','single_gust_recovery','repeated_gust','ramped_gust'):
        end=stop
        if s['profile']=='ramped_gust':require(0<ramp<stop-start,'ramp must be positive and leave a hold interval');end+=ramp
        if s['profile']=='repeated_gust':end=stop+Fraction(4,5)+stop-start
        require(end<=duration,'duration must include the complete wind command')
    if s['profile'].startswith('balanced_'):require(duration>=Fraction(12,5),'paired-boundary presets require at least 2.4 s')


def make_config(workers,s):
    valid_settings(s)
    c=configuration(workers,s['profile'],feedback=s['feedback'],speed=s['inlet_speed_m_s'],
                    duration=s['duration_s'],exchange=s['exchange_s'],gust_start=s['gust_start_s'],
                    gust_stop=s['gust_stop_s'],ramp=s['ramp_s'])
    if s['source_scale']!=1:c['source']['calibration']['fuel_kg_per_unit_m2']*=s['source_scale']
    if s['feedback']:c['return_channel']['sample_height_m']=s['sample_height_m']
    return c
