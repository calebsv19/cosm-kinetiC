"""Private periodic Euler adapter. Exact substep ticks; not a Fire return channel.

State pins the worker, adapter, gas/grid configuration and constant source packet.
Clock ownership remains with the caller; restart must keep the same substeps.
"""
import hashlib
import math
from pathlib import Path
import subprocess
import json
from fractions import Fraction
from surface_sources.growth_fire_v1 import digest, sealed, number
from . import caloric


def checked(value):
    if type(value) is not dict or value.get('digest') != digest(value):
        raise ValueError('hot-gas artifact identity')
    return {k: v for k, v in value.items() if k != 'digest'}


def rate(value):
    if (type(value) not in (list, tuple) or len(value) != 2 or
        any(type(v) is not int or not 1 <= v <= 10**9 for v in value)):
        raise ValueError('positive rational tick rate required')
    return Fraction(*value)


def validate(config):
    if type(config) is not dict or set(config)-{'caloric_moles'} != {'grid', 'length_m', 'gamma', 'gas_constant_j_kg_k'}:
        raise ValueError('hot-gas config fields')
    n, length = config['grid'], config['length_m']
    if type(n) is not list or len(n) != 3 or any(type(v) is not int or not 4 <= v <= 256 for v in n):
        raise ValueError('hot-gas grid')
    if math.prod(n) > 65536 or type(length) is not list or len(length) != 3:
        raise ValueError('hot-gas grid cap/length')
    for v in length: number(v, .001, 1000)
    gamma = number(config['gamma'], 1, 2)
    if gamma == 1 or number(config['gas_constant_j_kg_k'], 0) == 0:
        raise ValueError('hot-gas EOS domain')
    if 'caloric_moles' in config:
        reference=caloric.properties(config['caloric_moles'],298.15)
        if abs(gamma-reference['gamma'])>1e-12 or abs(config['gas_constant_j_kg_k']/reference['gas_constant_j_kg_k']-1)>1e-12:
            raise ValueError('caloric reference gamma/R metadata must match specified mixture')
    return 5*math.prod(n)


def array(value, count):
    if type(value) is not list or len(value) != count:
        raise ValueError('conserved array length')
    for v in value: number(v)


def primitive(config, values):
    count = validate(config); array(values, count)
    temperatures, pressures = [], []
    for q in range(0, count, 5):
        rho, mx, my, mz, energy = values[q:q+5]
        if rho <= 0: raise ValueError('positive density required')
        p = (config['gamma']-1)*(energy-(mx*mx+my*my+mz*mz)/(2*rho))
        t = p/(rho*config['gas_constant_j_kg_k'])
        if 'caloric_moles' in config:
            t=caloric.invert(config['caloric_moles'],(energy-(mx*mx+my*my+mz*mz)/(2*rho))/rho)
            p=rho*config['gas_constant_j_kg_k']*t
        if not math.isfinite(p) or not math.isfinite(t) or p <= 0 or t <= 0:
            raise ValueError('positive finite gas state required')
        temperatures.append(t); pressures.append(p)
    return temperatures, pressures


def identity(config, worker, source, tick_seconds):
    if 'caloric_moles' in config:raise ValueError('caloric EOS requires the domain worker protocol')
    count = validate(config); array(source, count); r = rate(tick_seconds)
    return dict(model='periodic_calorically_perfect_euler_v1', config_digest=digest(config),
        worker_sha256=hashlib.sha256(Path(worker).read_bytes()).hexdigest(),
        adapter_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        source_digest=digest({'integrated_per_substep': source}), tick_seconds=[r.numerator, r.denominator])


def initial(config, worker, conserved, source, tick_seconds, tick=0):
    if type(tick) is not int or not 0 <= tick <= 2**53: raise ValueError('exact nonnegative tick')
    primitive(config, conserved)
    return sealed(dict(schema='hotgas_checkpoint/v1', identity=identity(config, worker, source, tick_seconds),
                       tick=tick, conserved=list(conserved)))


def totals(config, values):
    volume = math.prod(config['length_m'])/math.prod(config['grid'])
    return [volume*math.fsum(values[i::5]) for i in range(5)]


def advance(config, worker, checkpoint, source, end_tick, tick_seconds):
    s = checked(checkpoint)
    if (set(s) != {'schema', 'identity', 'tick', 'conserved'} or s['schema'] != 'hotgas_checkpoint/v1' or
        s['identity'] != identity(config, worker, source, tick_seconds)):
        raise ValueError('hot-gas checkpoint configuration/worker/source/clock changed')
    if (type(s['tick']) is not int or type(end_tick) is not int or
        not 0 <= s['tick'] < end_tick <= 2**53 or end_tick-s['tick'] > 100000):
        raise ValueError('bounded advancing exact ticks')
    primitive(config, s['conserved'])
    steps = end_tick-s['tick']; dt = float(rate(tick_seconds))
    args = [*config['grid'], *config['length_m'], config['gamma'], config['gas_constant_j_kg_k'], dt, steps]
    proc = subprocess.run([str(Path(worker).resolve()), *[format(v, '.17g') for v in args]],
        input=' '.join(format(v, '.17g') for v in [*s['conserved'], *source]),
        text=True, capture_output=True, timeout=60)
    if proc.returncode: raise ValueError('native hot-gas worker refused: '+str(proc.returncode))
    result = json.loads(proc.stdout)
    if set(result) != {'conserved'}: raise ValueError('native hot-gas output fields')
    temps, pressures = primitive(config, result['conserved'])
    before, after, added = [totals(config, v) for v in (s['conserved'], result['conserved'], source)]
    expected = [a+steps*b for a, b in zip(before, added)]
    residual = [a-b for a, b in zip(after, expected)]
    for i in range(5):
        scale = max(1., abs(expected[i]), abs(before[i]), abs(steps*added[i]))
        if abs(residual[i]) > 1e-10*scale: raise ValueError('independent gas conservation budget')
    state = sealed(dict(s, tick=end_tick, conserved=result['conserved']))
    receipt = sealed(dict(schema='hotgas_step/v1', identity=s['identity'], prior_digest=checkpoint['digest'],
        next_digest=state['digest'], begin_tick=s['tick'], end_tick=end_tick,
        totals_before=before, totals_after=after, integrated_source=[steps*v for v in added],
        conservation_residual=residual, temperature_range_k=[min(temps), max(temps)],
        pressure_range_pa=[min(pressures), max(pressures)]))
    return state, receipt
