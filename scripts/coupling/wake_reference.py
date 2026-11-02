"""Controlled wake-grid and Fire-surface references; no solver changes."""
import copy
from surface_sources.growth_fire_v1 import require


def gas_grid(base, grid):
    """Add a 48³ control to the existing matched 64³/80³ wake series."""
    require(type(grid) is int and grid in (48, 64, 80), 'qualified wake grids')
    c = copy.deepcopy(base)
    f = c['fluid']
    require(f['length_m'] == [1, 1, 1] and f['momentum_dt_s'] == .0025,
            'fixed physical wake domain and gas timestep')
    require(all(v == 0 for k in ('initial_face_velocity_m_s', 'initial_energy_j',
                                'initial_smoke_kg') for v in f[k]),
            'still and scalar-free initial reference')
    require(f['solid_box']['lower_m'] == [.375]*3 and
            f['solid_box']['upper_m'] == [.625]*3, 'fixed aligned obstacle')
    require(c['receiver']['injection_depth_m'] == .125, 'fixed injection depth')
    f['grid'] = [grid]*3
    f['initial_face_velocity_m_s'] = [0.]*(3*grid**3+3*grid**2)
    for k in ('initial_energy_j', 'initial_smoke_kg'):
        f[k] = [0.]*grid**3
    f['resource_limits']['max_cells'] = grid**3
    f.pop('capacity_profile', None)
    if grid == 80:
        f['capacity_profile'] = 'wake80/v1'
    r = c['receiver']
    r['spacing_m'] = [1/grid]*3
    r['dimensions'] = [grid]*3
    r['fluid_mask'] = [1]*grid**2
    r['injection_fluid_mask'] = [1]*(grid**2*(grid//8))
    c['journal_codec'] = 'lossless_f64_zlib_1g/v1' if grid == 80 else 'lossless_f64_zlib/v1'
    return c


def fire_surface(base):
    """Double intervals on the Fire surface; preserve physical extent and rates."""
    c = copy.deepcopy(base)
    s = c['source']
    require(s['width'] == s['height'] == 17 and s['cell_size_m'] == .046875,
            'original 17-node physical source')
    require(s['dt_s'] == .02 and c['periods'] == [[1, 50], [1, 400], [1, 10]],
            'fixed solver and exchange clocks')
    require(c['fluid']['grid'] == [64]*3, 'fixed 64³ gas comparison')
    s['width'] = s['height'] = 33
    s['cell_size_m'] /= 2
    return c
