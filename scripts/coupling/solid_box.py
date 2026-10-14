"""One immutable grid-aligned insulated obstacle; independent native admission."""
import math
from surface_sources.growth_fire_v1 import require, keys, number

MODEL = 'xy_periodic_ground_box_open_top_projection3d_v1'


def topology(config):
    box = config.get('solid_box')
    if box is None:
        return [0] * math.prod(config['grid'])
    keys(box, ('schema', 'lower_m', 'upper_m', 'velocity_boundary', 'thermal_boundary'))
    require(box['schema'] == 'physics_sim_stationary_box/v1', 'solid box schema')
    require(box['velocity_boundary'] == 'no_slip' and box['thermal_boundary'] == 'insulated', 'solid boundary policy')
    require(config['boundary_policy']['vertical'] == 'solid_bottom_open_top', 'box requires ground/open top')
    bounds = []
    for name in ('lower_m', 'upper_m'):
        value = box[name]
        require(type(value) is list and len(value) == 3, 'box bounds')
        indices = []
        for x, length, count in zip(value, config['length_m'], config['grid']):
            number(x, 0, length)
            index = x * count / length
            require(abs(index - round(index)) <= 1e-10, 'box must align with native cell faces')
            indices.append(round(index))
        bounds.append(indices)
    lo, hi = bounds
    require(all(1 <= a < b < count for a, b, count in zip(lo, hi, config['grid'])), 'box must be interior')
    nx, ny, nz = config['grid']
    return [int(lo[0] <= i < hi[0] and lo[1] <= j < hi[1] and lo[2] <= k < hi[2])
            for k in range(nz) for j in range(ny) for i in range(nx)]


def blocked_faces(config, mask):
    nx, ny, nz = config['grid']; n = nx * ny * nz; plane = nx * ny
    blocked = []
    for axis in range(3):
        for k in range(nz + (axis == 2)):
            for j in range(ny):
                for i in range(nx):
                    q = (k * ny + j) * nx + i
                    right = 0 if k == nz else mask[q]
                    left = (mask[q - plane] if k else 0) if axis == 2 else mask[(k * ny + (j - 1) % ny) * nx + i] if axis == 1 else mask[(k * ny + j) * nx + (i - 1) % nx]
                    if right or left:
                        blocked.append(axis * n + q)
    return blocked


def check_state(config, data):
    if 'solid_box' not in config:
        return
    mask = topology(config)
    for q, solid in enumerate(mask):
        if solid:
            require(all(data[key][q] == 0 for key in ('energy_j', 'smoke_kg', 'pressure_pa')), 'nonzero solid cell state')
    require(all(data['face_velocity_m_s'][q] == 0 for q in blocked_faces(config, mask)), 'solid normal flow')


def check_sources(config, steps):
    mask = topology(config)
    for step in steps:
        require(all(not solid or (step['energy_j'][q] == 0 and step['smoke_kg'][q] == 0)
                    for q, solid in enumerate(mask)), 'source intersects solid')


def point_outside(config, point):
    box = config.get('solid_box')
    if box:
        require(not all(a <= x <= b for a, x, b in zip(box['lower_m'], point, box['upper_m'])), 'wind probe lies inside or on solid')
