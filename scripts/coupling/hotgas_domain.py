"""Conservative solid/reservoir gas domain and native conductive-plane sampling.

No inferred radiative flux, gauge response or reciprocal thermal return.
"""
import hashlib
import json
import math
from pathlib import Path
import subprocess
from . import hotgas, caloric
from surface_sources.growth_fire_v1 import number, digest, sealed

KINDS = {'periodic': 0, 'slip_wall': 1, 'reservoir': 2}
FACE_KEYS = {'kind', 'pressure_origin_pa', 'temperature_k', 'velocity_m_s', 'wall_temperature_k'}


def validate(config):
    if type(config) is not dict or set(config) != {'gas', 'domain'}:
        raise ValueError('domain configuration fields')
    count = hotgas.validate(config['gas']); domain = config['domain']
    required = {'faces', 'gravity_m_s2', 'conductivity_w_m_k'}
    if type(domain) is not dict or not required <= set(domain) or set(domain)-required-{'flux'}:
        raise ValueError('domain policy fields')
    if domain.get('flux', 'rusanov') not in ('rusanov', 'hllc', 'hllc_muscl', 'lhllc_muscl'):
        raise ValueError('qualified flux policy names required')
    hotgas.array(domain['gravity_m_s2'], 3)
    for v in domain['gravity_m_s2']: number(v, -1000, 1000)
    number(domain['conductivity_w_m_k'], 0, 100)
    if type(domain['faces']) is not list or len(domain['faces']) != 6:
        raise ValueError('six boundary faces required')
    for face in domain['faces']:
        if type(face) is not dict or set(face) != FACE_KEYS or face['kind'] not in KINDS:
            raise ValueError('domain face fields')
        if number(face['pressure_origin_pa'], 0) == 0 or number(face['temperature_k'], 0) == 0:
            raise ValueError('positive reservoir pressure/temperature')
        hotgas.array(face['velocity_m_s'], 3); number(face['wall_temperature_k'], 0)
        if face['kind'] != 'slip_wall' and face['wall_temperature_k'] != 0:
            raise ValueError('wall thermal reservoir belongs to solid face')
    for axis in range(3):
        periodic = [domain['faces'][2*axis+i]['kind'] == 'periodic' for i in (0, 1)]
        if periodic[0] != periodic[1] or (periodic[0] and domain['gravity_m_s2'][axis] != 0):
            raise ValueError('paired periodic axis with single-valued potential required')
    if 'caloric_moles' in config['gas']:
        for face in domain['faces']:
            if face['kind']=='reservoir':caloric.temperature(face['temperature_k'])
            if face['wall_temperature_k']:caloric.temperature(face['wall_temperature_k'])
    return count


def identity(config, worker, source, tick_seconds):
    count = validate(config); hotgas.array(source, count); rate = hotgas.rate(tick_seconds)
    model = 'solid_reservoir_gravity_conduction_hllc_v1' if config['domain'].get('flux') == 'hllc' else 'solid_reservoir_gravity_conduction_euler_v1'
    if config['domain'].get('flux') == 'hllc_muscl': model = 'solid_reservoir_gravity_conduction_muscl_rk2_v1'
    if config['domain'].get('flux') == 'lhllc_muscl': model = 'solid_reservoir_gravity_conduction_lhllc_compression_guarded_rk3_v1'
    if 'caloric_moles' in config['gas']:model+='__frozen_caloric_nist_gri_v1'
    return dict(model=model, config_digest=digest(config),
        worker_sha256=hashlib.sha256(Path(worker).read_bytes()).hexdigest(),
        adapter_sha256=digest({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in ((Path(__file__), Path(hotgas.__file__), Path(caloric.__file__), Path(caloric.nist.__file__)) if 'caloric_moles' in config['gas'] else (Path(__file__), Path(hotgas.__file__)))}),
        source_digest=digest({'integrated_per_substep': source}), tick_seconds=[rate.numerator, rate.denominator])


def initial(config, worker, conserved, source, tick_seconds, tick=0):
    validate(config); hotgas.primitive(config['gas'], conserved)
    if type(tick) is not int or not 0 <= tick <= 2**53: raise ValueError('exact nonnegative tick')
    return sealed(dict(schema='hotgas_domain_checkpoint/v1',
        identity=identity(config, worker, source, tick_seconds), tick=tick, conserved=list(conserved)))


def potential_total(config, conserved):
    gas = config['gas']; grid = gas['grid']; gravity = config['domain']['gravity_m_s2']
    volume = math.prod(gas['length_m'])/math.prod(grid)
    terms = []
    for q in range(math.prod(grid)):
        stride = 1; phi = 0.
        for axis in range(3):
            phi -= gravity[axis]*(q//stride % grid[axis]+.5)*gas['length_m'][axis]/grid[axis]
            stride *= grid[axis]
        terms.append(conserved[5*q]*phi)
    return volume*math.fsum(terms)


def native(config, worker, conserved, source, dt, steps, planes):
    count = validate(config); hotgas.array(source, count); hotgas.primitive(config['gas'], conserved)
    if type(steps) is not int or not 0 <= steps <= 100000 or number(dt, 0) == 0:
        raise ValueError('bounded native substeps and positive dt required')
    if type(planes) is not list or len(planes) > 8: raise ValueError('bounded plane list')
    gas = config['gas']; domain = config['domain']
    for height in planes: number(height, 0, gas['length_m'][2]-.5*gas['length_m'][2]/gas['grid'][2])
    data = [*domain['gravity_m_s2'], domain['conductivity_w_m_k']]
    for face in domain['faces']:
        data += [KINDS[face['kind']], face['pressure_origin_pa'], face['temperature_k'],
                 *face['velocity_m_s'], face['wall_temperature_k']]
    data += [*conserved, *source, len(planes), *planes]
    if 'caloric_moles' in gas:data=gas['caloric_moles']+data
    args = [*gas['grid'], *gas['length_m'], gas['gamma'], gas['gas_constant_j_kg_k'], dt, steps]
    flux_args = [domain['flux']] if domain.get('flux') in ('hllc', 'hllc_muscl', 'lhllc_muscl') else []
    if 'caloric_moles' in gas:flux_args=[domain.get('flux','rusanov'),'caloric']
    proc = subprocess.run([str(Path(worker).resolve()), *[format(v, '.17g') for v in args], *flux_args],
        input=' '.join(format(v, '.17g') for v in data), text=True, capture_output=True, timeout=60)
    if proc.returncode: raise ValueError('native gas domain refused: '+str(proc.returncode))
    result = json.loads(proc.stdout)
    if type(result) is not dict or set(result) != {'conserved', 'budget', 'samples'}:
        raise ValueError('native domain output fields')
    hotgas.primitive(gas, result['conserved'])
    budget = result['budget']
    if type(budget) is not dict or set(budget) != {'outward', 'potential_outward_j', 'conducted_outward_j', 'gravity_impulse_kg_m_s', 'gravity_work_j'}:
        raise ValueError('native boundary ledger fields')
    if type(budget['outward']) is not list or len(budget['outward']) != 6:
        raise ValueError('six outward ledgers')
    for row in budget['outward']: hotgas.array(row, 5)
    hotgas.array(budget['potential_outward_j'], 6); hotgas.array(budget['conducted_outward_j'], 6)
    hotgas.array(budget['gravity_impulse_kg_m_s'], 3); number(budget['gravity_work_j'])
    if type(result['samples']) is not list or len(result['samples']) != len(planes):
        raise ValueError('native plane count')
    for height, sample in zip(planes, result['samples']):
        if (type(sample) is not dict or set(sample) != {'height_m', 'temperature_k', 'conducted_downward_w_m2', 'advected_upward_w_m2'} or
            number(sample['height_m']) != height): raise ValueError('native plane identity')
        for key in ('temperature_k', 'conducted_downward_w_m2', 'advected_upward_w_m2'):
            hotgas.array(sample[key], gas['grid'][0]*gas['grid'][1])
        if min(sample['temperature_k']) <= 0: raise ValueError('positive sampled temperature')
    return result


def state(config, worker, checkpoint, source, tick_seconds):
    s = hotgas.checked(checkpoint)
    if (set(s) != {'schema', 'identity', 'tick', 'conserved'} or s['schema'] != 'hotgas_domain_checkpoint/v1' or
        s['identity'] != identity(config, worker, source, tick_seconds) or
        type(s['tick']) is not int or not 0 <= s['tick'] <= 2**53):
        raise ValueError('domain checkpoint identity changed')
    hotgas.primitive(config['gas'], s['conserved'])
    return s


def observe(config, worker, checkpoint, source, tick_seconds, planes):
    s = state(config, worker, checkpoint, source, tick_seconds)
    result = native(config, worker, s['conserved'], source, float(hotgas.rate(tick_seconds)), 0, planes)
    if result['conserved'] != s['conserved'] or identity(config, worker, source, tick_seconds) != s['identity']:
        raise ValueError('observation altered state or controls')
    return sealed(dict(schema='hotgas_conductive_plane_observation/v1', checkpoint_digest=checkpoint['digest'],
        tick=s['tick'], identity=s['identity'], native_samples=result['samples'],
        coordinate_frame='Cartesian +z upward, z=0 prescribed flat surface',
        conducted_sign='positive downward into lower surface', advected_sign=('positive upward sensible enthalpy plus kinetic energy, h(298.15 K)=0' if 'caloric_moles' in config['gas'] else 'positive upward total enthalpy transport'),
        radiative_flux_w_m2=None, experimental_gauge_response_qualified=False,
        reciprocal_thermal_exchange_admissible=False))


def advance(config, worker, checkpoint, source, end_tick, tick_seconds):
    s = state(config, worker, checkpoint, source, tick_seconds)
    if type(end_tick) is not int or not s['tick'] < end_tick <= min(2**53, s['tick']+100000):
        raise ValueError('bounded exact advancing ticks required')
    steps = end_tick-s['tick']
    result = native(config, worker, s['conserved'], source, float(hotgas.rate(tick_seconds)), steps, [])
    if identity(config, worker, source, tick_seconds) != s['identity']:
        raise ValueError('domain controls changed during execution')
    gas = config['gas']; budget = result['budget']
    before, after, added = [hotgas.totals(gas, v) for v in (s['conserved'], result['conserved'], source)]
    out = [math.fsum(row[i] for row in budget['outward']) for i in range(5)]
    gravity = [0., *budget['gravity_impulse_kg_m_s'], budget['gravity_work_j']]
    expected = [a+steps*b-c+d for a, b, c, d in zip(before, added, out, gravity)]
    residual = [a-b for a, b in zip(after, expected)]
    for i in range(5):
        if abs(residual[i]) > 1e-10*max(1., abs(before[i]), abs(expected[i]), abs(out[i]), abs(gravity[i])):
            raise ValueError('domain mass/momentum/gas energy ledger failed')
    phi_before = potential_total(config, s['conserved']); phi_after = potential_total(config, result['conserved'])
    source_phi = steps*potential_total(config, source)
    potential_residual = after[4]+phi_after-before[4]-phi_before-steps*added[4]-source_phi+out[4]+math.fsum(budget['potential_outward_j'])
    if abs(potential_residual) > 1e-10*max(1., abs(before[4]), abs(phi_before), abs(out[4])):
        raise ValueError('independent gas plus gravitational potential balance failed')
    final = sealed(dict(s, tick=end_tick, conserved=result['conserved']))
    receipt = sealed(dict(schema='hotgas_domain_step/v1', prior_digest=checkpoint['digest'], next_digest=final['digest'],
        identity=s['identity'], begin_tick=s['tick'], end_tick=end_tick, boundary_budget=budget,
        totals_before=before, totals_after=after, integrated_source=[steps*v for v in added],
        conservation_residual=residual, potential_energy_before_j=phi_before, potential_energy_after_j=phi_after,
        source_potential_energy_j=source_phi, gas_plus_potential_residual_j=potential_residual))
    return final, receipt
