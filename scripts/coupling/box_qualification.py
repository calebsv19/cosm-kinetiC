"""Matched inert-box plume controls, native wind probes and spatial refinement."""
import copy
import json
import math
import shutil
from pathlib import Path

from surface_sources.growth_fire_v1 import require, sealed
from .framework import Framework
from .workbench import make_config, DEFAULTS, read_frames
from .solid_box import topology, blocked_faces, point_outside
from .velocity import sample_mac
from .allocation import allocate_window
from .convergence import matched_comparisons, restrict
from .scenario import metrics
from open_atmosphere import run, SCHEMA

BOX = {'schema': 'physics_sim_stationary_box/v1', 'lower_m': [.125, .125, .375],
       'upper_m': [.375, .375, .625], 'velocity_boundary': 'no_slip', 'thermal_boundary': 'insulated'}
PROBES = {'below': [.25, .25, .3125], 'above': [.25, .25, .6875],
          'left': [.0625, .25, .5], 'right': [.4375, .25, .5],
          'front': [.25, .0625, .5], 'back': [.25, .4375, .5]}


def configuration(workers, grid=8, box=True, duration=3.2):
    config = make_config(workers, dict(DEFAULTS, profile='warm_plume', wind_scale=1., source_scale=3., duration_s=duration))
    # Fixed lower heat transfer on every mesh; the original .75 coarse-grid
    # demonstration exceeds the contrast envelope under refinement.
    config['source']['calibration']['energy_transfer_fraction'] = .1
    f = config['fluid']; n = grid ** 3
    f['grid'] = [grid] * 3
    # Both controls start from rest: identical valid initial flow, including the solid.
    f['initial_face_velocity_m_s'] = [0.] * (3 * n + grid * grid)
    f['initial_energy_j'] = [0.] * n; f['initial_smoke_kg'] = [0.] * n
    config['receiver'].update(spacing_m=[1 / grid] * 3, dimensions=[grid] * 3,
                              fluid_mask=[1] * (grid * grid),
                              injection_fluid_mask=[1] * (grid * grid * (grid // 8)))
    if box: f['solid_box'] = copy.deepcopy(BOX)
    return config


def probes(config, result):
    f = config['fluid']; fields = result['fields']
    values = {}
    for name, point in PROBES.items():
        point_outside(f, point)
        values[name] = {'position_m': point, 'velocity_m_s': sample_mac(fields['face_velocity_m_s'], f['grid'], f['length_m'], point, solid_floor=True)}
    return values


def checks(config, result):
    f = config['fluid']; fields = result['fields']; mask = topology(f)
    blocked = blocked_faces(f, mask)
    solid_totals = {key: math.fsum(v for v, solid in zip(fields[key], mask) if solid) for key in ('energy_j', 'smoke_kg')}
    maximum = max((abs(fields['face_velocity_m_s'][q]) for q in blocked), default=0.)
    require(maximum == 0 and all(v == 0 for v in solid_totals.values()), 'solid exclusion')
    return {'blocked_faces': len(blocked), 'max_blocked_normal_velocity_m_s': maximum,
            'solid_cells': sum(mask), 'solid_totals': solid_totals,
            'max_divergence_s_inv': fields['max_divergence_s_inv'],
            'projection_relative_residual': fields['projection_relative_residual'],
            'budgets': result['budgets'], 'wind_probes': probes(config, result)}


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def coupled(workers, root, box):
    config = configuration(workers, box=box); write(root / 'config.json', config)
    host = Framework(config, root / 'store'); host.initialize()
    host.step(0, 'window-0')
    before = host.path.read_bytes()
    require(host.step(0, 'window-0')['revision'] == 1 and host.path.read_bytes() == before, 'same-operation replay')
    def fail(_): raise RuntimeError('deliberate staged failure')
    try: host.step(1, 'window-1', before_publish=fail)
    except RuntimeError: pass
    else: raise ValueError('staged failure did not execute')
    require(host.path.read_bytes() == before, 'failure preserves accepted joint store')
    restart = root / 'restart'; restart.mkdir(); shutil.copy2(host.path, restart / 'joint.sqlite3')
    other = Framework(config, restart)
    require(host.step(1, 'window-1') == other.step(1, 'window-1'), 'exact checkpoint recovery')
    require(host.inspect() == other.inspect(), 'full native recovered fields')
    for revision in range(2, config['windows']): host.step(revision, 'window-' + str(revision))
    cp = host.inspect(); write(root / 'head.json', cp)
    rows = read_frames(host)
    report = {'metrics': metrics(cp), 'checks': checks(config, cp['fluid']),
              'joint_checkpoint_digest': cp['digest'], 'replay': True, 'recovery': True, 'rollback': True,
              'history': [{'revision': row['revision'], 'time_s': row['fluid']['fields']['time_s'],
                           'checks': checks(config, row['fluid'])} for row in rows]}
    require(all(row['evidence']['held_return']['applied_to_fire'] for row in rows[1:]), 'held wind enabled')
    write(root / 'checks.json', report)
    return host, rows, report


def frozen(workers, root, frames, schedule, grid, box):
    config = configuration(workers, grid=grid, box=box)
    # Deliberately replay one accepted Fire source sequence for all six Fluid cases.
    host = Framework(config, root / 'unused'); state = None; result = None; batch = []
    for frame, window in zip(frames, schedule):
        policy = {'schema': 'physics_sim_surface_source_policy/v1', 'consumer_id': 'box-refinement',
                  'model_id': 'matched-frozen-fire', 'source_config': config['source'],
                  'producer': frame['producer'],
                  'stream_start_tick': 0, 'receiver': config['receiver']}
        from surface_sources.growth_fire_v1 import frame_valid
        frame_valid(frame)
        batch.extend(allocate_window(frame, policy, window))
        if len(batch) >= 80:
            result = run(dict(config['fluid'], schema=SCHEMA, state=state, steps=batch), workers[2])
            state = result['state']; batch = []
    if batch: result = run(dict(config['fluid'], schema=SCHEMA, state=state, steps=batch), workers[2])
    host.unchanged(); write(root / 'config.json', config); write(root / 'fluid.json', result)
    report = checks(config, result); write(root / 'checks.json', report)
    return result, report


def spatial(results, reports):
    grids = [[g] * 3 for g in (8, 16, 32)]
    pairs, native = matched_comparisons([v['fields'] for v in results], grids)
    ratios = {key: pairs[1][key]['relative_l1'] / pairs[0][key]['relative_l1'] for key in ('energy_j', 'smoke_kg')}
    probe_vectors = [[component for row in report['wind_probes'].values() for component in row['velocity_m_s']] for report in reports]
    differences = [math.sqrt(math.fsum((a - b) ** 2 for a, b in zip(probe_vectors[i], probe_vectors[i+1]))) for i in (0, 1)]
    return {'common_partition_pairs': pairs, 'native_partition_pairs': native,
            'fine_to_coarse_l1_ratio': ratios, 'probe_velocity_difference_l2_m_s': differences,
            'scalar_differences_decrease': all(v < 1 for v in ratios.values()),
            'probe_differences_decrease': differences[1] < differences[0],
            'asymptotic_order_qualified': False, 'physical_calibration_qualified': False}
