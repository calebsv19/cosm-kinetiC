"""Local Fire–Fluid authoring and accepted-checkpoint playback, using native adapters."""
import base64
from contextlib import closing
import json
import math
from pathlib import Path
import sqlite3
import subprocess
import sys
import threading
import uuid
from fractions import Fraction

from cfd_evidence import admitted_path
from surface_sources.growth_fire_v1 import require, keys, number, integer
from fire_fluid_spatial import ground_configuration
from .qualification import case
from .framework import Framework
from .scenario import metrics, baseline

INERT_BOX = {'schema': 'physics_sim_stationary_box/v1', 'lower_m': [.125, .125, .375],
             'upper_m': [.375, .375, .625], 'velocity_boundary': 'no_slip', 'thermal_boundary': 'insulated'}

DEFAULTS = {'feedback': True, 'wind_scale': 20., 'source_scale': 1.,
            'sample_height_m': .0625, 'exchange_s': .1, 'duration_s': 2.}


def settings_valid(settings):
    keys(settings, (*DEFAULTS, 'profile') if 'profile' in settings else tuple(DEFAULTS))
    require(settings.get('profile', 'baseline') in ('baseline', 'warm_plume', 'box_control', 'inert_box','isolated_control','isolated_box'), 'unknown scene profile')
    if settings.get('profile') in ('box_control', 'inert_box','isolated_control','isolated_box'):
        require(settings['feedback'] and settings['wind_scale']==1 and settings['source_scale']==3
                and settings['sample_height_m']==.0625 and settings['exchange_s']==.1, 'box qualification preset requires feedback, wind 1, source 3, height .0625 and exchange .1')
    require(type(settings['feedback']) is bool, 'feedback must be boolean')
    for key, low, high in (('wind_scale', 0, 40), ('source_scale', .1, 5),
                           ('sample_height_m', .001, .499), ('duration_s', .1, 3.2)):
        number(settings[key], low, high)
    require(settings['exchange_s'] in (.02, .04, .1), 'exchange must be 0.02, 0.04 or 0.1 seconds')
    count = Fraction(str(settings['duration_s'])) / Fraction(str(settings['exchange_s']))
    require(count.denominator == 1, 'duration must end on an exchange boundary')
    integer(count.numerator, 3, 32)


def make_config(workers, settings):
    settings_valid(settings)
    if settings.get('profile') in ('isolated_control','isolated_box'):
        from .isolated_qualification import configuration
        return configuration(workers,box=settings['profile']=='isolated_box',duration=settings['duration_s'])
    config = ground_configuration(baseline(*workers, nodes=8))
    config['return_channel'] = {'schema': 'fire_fluid_prior_velocity_hold/v1', 'sample_height_m': .0625}
    config = case(config, height=settings['sample_height_m'],
                  exchange=Fraction(str(settings['exchange_s'])),
                  duration=Fraction(str(settings['duration_s'])), wind_scale=settings['wind_scale'],
                  domain_height=1. if settings.get('profile') in ('warm_plume','box_control','inert_box') else .5)
    config['source']['evolution']['model'] = 'physical_rates_heat_outflow_v1'
    config['source']['calibration']['fuel_kg_per_unit_m2'] *= settings['source_scale']
    if settings.get('profile') in ('warm_plume','box_control','inert_box'):
        config['source']['calibration']['energy_transfer_fraction'] = .75
    if settings.get('profile') in ('box_control','inert_box'):
        config['source']['calibration']['energy_transfer_fraction'] = .1
        config['fluid']['initial_face_velocity_m_s'] = [0.]*len(config['fluid']['initial_face_velocity_m_s'])
        if settings['profile']=='inert_box': config['fluid']['solid_box']=json.loads(json.dumps(INERT_BOX))
    if not settings['feedback']:
        config.pop('return_channel')
        config['source']['evolution']['model'] = 'physical_rates_v1'
    return config


def read_frames(host):
    # Validate the entire accepted chain before presenting any record.
    host.inspect()
    with closing(sqlite3.connect(host.path.as_uri() + '?mode=ro', uri=True)) as db:
        return [json.loads(row[0]) for row in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]


def visual_frame(host, cp):
    native = host.fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
    fields = cp['fluid']['fields']; grid = host.config['fluid']['grid']
    nx, ny, nz = grid; plane = nx * ny
    held = cp['evidence'].get('held_return')
    return {'revision': cp['revision'], 'checkpoint_digest': cp['digest'],
            'time_s': fields['time_s'], 'metrics': metrics(cp),
            'surface': {'dimensions': [host.config['source']['width'], host.config['source']['height']],
                        'fields': {key: native['fields'][key] for key in ('fuel', 'burn', 'heat', 'ash')}},
            'fluid': {'grid': grid, 'length_m': host.config['fluid']['length_m'],
                      'temperature_column_peak_k': [max(fields['temperature_k'][k * plane + q] for k in range(nz)) for q in range(plane)],
                      'smoke_column_kg_m3': [math.fsum(fields['smoke_concentration_kg_m3'][k * plane + q] for k in range(nz)) / nz for q in range(plane)],
                      'smoke_side_kg_m3': [max(fields['smoke_concentration_kg_m3'][k * plane + j * nx + i] for j in range(ny)) for k in range(nz) for i in range(nx)],
                      'smoke_above_first_layer_fraction': math.fsum(fields['smoke_kg'][plane:]) / max(math.fsum(fields['smoke_kg']), 1e-300)},
            'feedback': None if held is None else {
                'prior_checkpoint_digest': held['observation']['joint_checkpoint_digest'],
                'sample_height_m': held['observation']['sample_height_m'],
                'window_ticks': held['held_window_ticks'], 'ticks_per_second': cp['evidence']['rate'],
                'tangent_velocity_m_s': held['wind_input']['tangent_velocity_m_s'], 'applied_to_fire': True}}


class Workbench:
    def __init__(self, root, workers):
        self.root = admitted_path(root); self.root.mkdir(parents=True, exist_ok=True)
        self.workers = tuple(str(admitted_path(p)) for p in workers)
        self.lock = threading.RLock(); self.playing = False; self.error = None
        self.wake = threading.Event(); self.closed = False; self.host = None; self.run_id = None
        if (self.root / 'current.json').exists():
            self.resume(json.loads((self.root / 'current.json').read_text())['run_id'])

    def resume(self, run_id):
        require(type(run_id) is str and len(run_id) == 36 and str(uuid.UUID(run_id)) == run_id, 'run identity')
        with self.lock:
            require(not self.playing, 'pause before opening a saved run')
            run = admitted_path(self.root / 'runs' / run_id)
            host = Framework(json.loads((run / 'config.json').read_text()), run / 'store')
            host.inspect()
            settings = json.loads((run / 'settings.json').read_text()); settings_valid(settings)
            self.host = host; self.run_id = run_id; self.error = None; self.settings = settings
            self._pointer()

    def _pointer(self):
        path = self.root / 'current.json'; pending = path.with_suffix('.pending')
        pending.write_text(json.dumps({'run_id': self.run_id}) + '\n'); pending.replace(path)

    def create(self, settings):
        with self.lock:
            require(not self.playing, 'pause before creating a scene')
            config = make_config(self.workers, settings)
            run_id = str(uuid.uuid4()); run = self.root / 'runs' / run_id
            host = Framework(config, run / 'store')  # admission before files
            run.mkdir(parents=True, exist_ok=False)
            for name, value in (('settings', settings), ('config', config)):
                (run / (name + '.json')).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')
            self._native_command(host, 'init')
            self.host = host; self.run_id = run_id; self.settings = dict(settings); self.error = None
            self._pointer()
            return self.state()

    def state(self):
        with self.lock:
            require(self.host is not None, 'create a scene first')
            frames = read_frames(self.host)
            return {'schema': 'fire_fluid_workbench_state/v1', 'run_id': self.run_id,
                    'playing': self.playing, 'error': self.error, 'settings': self.settings,
                    'preview': json.loads((self.host.path.parent.parent / 'preview.json').read_text())
                               if (self.host.path.parent.parent / 'preview.json').is_file() else None,
                    'windows': self.host.config['windows'], 'revision': frames[-1]['revision'],
                    'complete': frames[-1]['revision'] == self.host.config['windows'],
                    'frames': [visual_frame(self.host, cp) for cp in frames],
                    'saved_runs': sorted(p.name for p in (self.root / 'runs').iterdir()
                                         if (p / 'config.json').is_file() and (p / 'store/joint.sqlite3').is_file()
                                         and (p / 'store/joint.sqlite3').stat().st_size > 0),
                    'physical_calibration_qualified': False, 'thermal_return_enabled': False}

    def step(self, revision, operation_id):
        with self.lock:
            require(self.host is not None, 'create a scene first')
            return self._native_command(self.host, 'step', '--revision', str(revision), '--operation-id', operation_id)

    def _native_command(self, host, action, *arguments):
        # Native supervision owns signals; execute in a child main interpreter,
        # never in an HTTP or playback thread. Source/config pins remain unchanged.
        command = [sys.executable, '-B', str(Path(__file__).resolve().parents[1] / 'fire_fluid.py'),
                   '--config', str(host.path.parent.parent / 'config.json'), '--store', str(host.path.parent), action, *arguments]
        result = subprocess.run(command, capture_output=True, text=True, timeout=75)
        if result.returncode:
            if 'Build held by active cleanup or another build:' in result.stderr:
                raise ValueError('Native worker is in use by a build. Accepted state is preserved; retry after the build finishes.')
            detail = result.stderr.strip().splitlines()[-1] if result.stderr.strip() else 'Native operation failed'
            raise ValueError(detail)
        return json.loads(result.stdout)

    def play(self):
        with self.lock:
            require(self.host is not None, 'create a scene first')
            require(self.host.inspect()['revision'] < self.host.config['windows'], 'scene has completed')
            self.error = None; self.playing = True; self.wake.set()

    def pause(self):
        with self.lock:
            self.playing = False

    def run_loop(self):
        while not self.closed:
            self.wake.wait(.1); self.wake.clear()
            with self.lock:
                if self.closed: return
                if not self.playing: continue
                try:
                    cp = self.host.inspect()
                    if cp['revision'] == self.host.config['windows']:
                        self.playing = False; continue
                    self.step(cp['revision'], 'play-' + str(cp['revision']))
                    if cp['revision'] + 1 == self.host.config['windows']: self.playing = False
                except Exception as error:
                    self.error = str(error); self.playing = False

    def export(self):
        from .playback_export import export_playback
        with self.lock:
            require(not self.playing, 'pause before exporting')
            require(self.host is not None, 'create a scene first')
            return export_playback(self.host, self.root / 'exports' / (self.run_id + '-' + str(uuid.uuid4())))

    def close(self):
        with self.lock:
            self.playing = False; self.closed = True; self.wake.set()
