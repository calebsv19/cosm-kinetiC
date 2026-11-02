"""Physical-coordinate wind refinement; extensive fields share one partition."""
import copy
import math
from fractions import Fraction
from .convergence import comparison
from .open_boundaries import layout
from .velocity import sample_mac
from surface_sources.growth_fire_v1 import require


def refined(config, grid):
    """Preserve physical source, injection support, box and inlet profile."""
    c = copy.deepcopy(config)
    require(type(grid) is int and grid in (8, 16, 32, 64), 'wind comparison grid')
    f = c['fluid']
    require(f['length_m'] == [1, 1, 1], 'unit physical comparison domain')
    require(all(v == 0 for v in f['initial_face_velocity_m_s']), 'still initial reference')
    require(all(v == 0 for key in ('initial_energy_j', 'initial_smoke_kg') for v in f[key]),
            'zero initial scalar reference')
    f['grid'] = [grid] * 3
    f['initial_face_velocity_m_s'] = [0.] * layout(f)[0]
    for key in ('initial_energy_j', 'initial_smoke_kg'):
        f[key] = [0.] * grid**3
    r = c['receiver']
    r['spacing_m'] = [1 / grid] * 3
    r['dimensions'] = [grid] * 3
    depth = r['injection_depth_m'] * grid
    require(depth == int(depth), 'same physical injection depth on nested grids')
    r['fluid_mask'] = [1] * grid**2
    r['injection_fluid_mask'] = [1] * (grid**2 * int(depth))
    return c


def cadence(config, seconds, duration):
    """Change exchange only; native solver clocks and physical forcing stay fixed."""
    c = copy.deepcopy(config)
    seconds = Fraction(seconds)
    duration = Fraction(duration)
    count = duration / seconds
    require(count.denominator == 1 and 3 <= count <= 32, 'bounded aligned comparison')
    for period in c['periods'][:2]:
        require((seconds / Fraction(*period)).denominator == 1, 'aligned native cadence')
    c['periods'][2] = [seconds.numerator, seconds.denominator]
    c['windows'] = int(count)
    return c


def fixed_probes():
    return {
        'under_box': [.5, .5, .3125], 'over_box': [.5, .5, .6875],
        'upstream': [.3125, .5, .5], 'downstream': [.6875, .5, .5],
        'front': [.5, .3125, .5], 'back': [.5, .6875, .5],
        'fire_return_center': [.5, .5, .125],
    }


def sample(fields, config):
    f = config['fluid']
    return {name: sample_mac(fields['face_velocity_m_s'], f['grid'], f['length_m'], point,
                             solid_floor=True, lateral_open=True)
            for name, point in fixed_probes().items()}


def field_center(fields, config, key):
    """Physical centroid from extensive cell quantities; no index-space fit."""
    grid=config['fluid']['grid'];length=config['fluid']['length_m'];values=fields[key]
    require(len(values)==math.prod(grid),'centroid dimensions')
    total=math.fsum(values)
    if total==0:return None
    strides=(1,grid[0],grid[0]*grid[1])
    return [math.fsum(v*((q//strides[axis])%grid[axis]+.5)*length[axis]/grid[axis]
                     for q,v in enumerate(values))/total for axis in range(3)]


def compare(coarse, fine, coarse_config, fine_config):
    a = coarse['fluid']['fields']
    b = fine['fluid']['fields']
    require(math.isclose(a['time_s'], b['time_s'], abs_tol=1e-12), 'matched physical endpoint')
    require(coarse_config['source'] == fine_config['source'], 'fixed physical Fire configuration')
    for key in ('properties', 'length_m', 'wind_forcing'):
        require(coarse_config['fluid'][key] == fine_config['fluid'][key], 'fixed physical ' + key)
    require(coarse_config['receiver']['injection_depth_m'] == fine_config['receiver']['injection_depth_m'],
            'fixed physical source depth')
    # Restrict extensive mass/energy by sums, never interpolate per-cell totals.
    fields = comparison(a, b, coarse_config['fluid']['grid'], fine_config['fluid']['grid'])
    pa = sample(a, coarse_config)
    pb = sample(b, fine_config)
    n = 3 * len(pa)
    difference = math.sqrt(math.fsum((x-y)**2 for name in pa for x,y in zip(pa[name],pb[name])) / n)
    scale = math.sqrt(math.fsum(x*x for values in pb.values() for x in values) / n)
    return {'time_s': a['time_s'], 'extensive_fields': fields,
            'probe_velocity_rms_difference_m_s': difference,
            'probe_velocity_relative_rms_difference': difference / scale if scale else None,
            'probe_positions_m': fixed_probes(), 'coarse_probes_m_s': pa, 'fine_probes_m_s': pb,
            'smoke_centroid_m': {'coarse':field_center(a,coarse_config,'smoke_kg'),'fine':field_center(b,fine_config,'smoke_kg')},
            'gas_heat_centroid_m': {'coarse':field_center(a,coarse_config,'energy_j'),'fine':field_center(b,fine_config,'energy_j')},
            'heat_outflow_j': {'coarse':coarse['fluid']['budgets']['energy_j']['outflow'],'fine':fine['fluid']['budgets']['energy_j']['outflow']},
            'smoke_outflow_kg': {'coarse':coarse['fluid']['budgets']['smoke_kg']['outflow'],'fine':fine['fluid']['budgets']['smoke_kg']['outflow']},
            'physical_calibration_qualified': False,
            'spatial_convergence_qualified': False}
