"""Durable one-way Fluid control consuming an immutable retained Fire pulse.

This is a diagnostic participant, not a new live Fire/Fluid coupling policy.
Every native result uses the ordinary acceptance and restart path. SQLite
publishes exact checkpoints before successful copied native staging retires.
"""
from contextlib import closing
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import sqlite3

from atmosphere_attempt import CONTROLS
from numeric_digest_stream import digest, sealed
from surface_sources.growth_fire_v1 import require
from . import fluid_adapter
from .fluid_packets import run
from .journal import Journal
from .live_gust import retire_attempts
from .source_replay import Replay


class ReplayRun:
    def __init__(self, root, config, pulse, *, packet_steps=20, deposition_interval=None, io_format=None, journal_mode=None):
        self.root = Path(root).resolve()
        self.path = self.root / 'replay.sqlite3'
        self.config = config
        self.worker = Path(config['fluid_worker'])
        from .scalar_gust128 import selected,Source
        from .scalar_wake128 import selected as selected_wake128,Source as WakeSource
        from .scalar_wake128_work import selected as selected_work128
        wake128=selected_wake128(config['fluid']) or selected_work128(config['fluid'])
        profile128=selected(config['fluid']) or wake128
        if wake128:
            require(getattr(self,'_bounded_retention',False),'long128 replay requires bounded retention')
            Source=WakeSource
        self.replay = (Source if profile128 else Replay)(pulse, config['receiver'], deposition_interval=deposition_interval)
        self.packet_steps = packet_steps
        require(io_format in (None,'field_packet/v1'),'replay native IO format')
        self.io_format=io_format
        self.dt = Fraction(*config['periods'][1])
        require(float(self.dt) == config['fluid']['momentum_dt_s'], 'replay native clock')
        require(type(packet_steps) is int and 1 <= packet_steps <= 80, 'replay packet bound')
        fluid_adapter.validate(config['fluid'])
        self.codec = Journal(journal_mode or 'lossless_f64_zlib_1g/v1')
        if profile128:
            from .journal import FIELD_PACKET_2G
            require(io_format=='field_packet/v1' and packet_steps<=(2 if wake128 else 4) and self.codec.mode==FIELD_PACKET_2G,'closed128 replay IO/storage bundle')
        scripts = Path(__file__).resolve().parents[1]
        files = [scripts / n for n in (*CONTROLS, 'surface_sources/receiver.py',
                 'coupling/source_replay.py', 'coupling/transport_replay.py',
                 'coupling/fluid_packets.py', 'coupling/fluid_adapter.py',
                 'coupling/frozen_transport.py', 'coupling/solid_box.py',
                 'coupling/open_boundaries.py', 'coupling/source_timing.py')]
        self.files = files + [self.worker]
        if selected_work128(config['fluid']):
            self.files += [scripts/'coupling/scalar_wake128_work.py',
                           scripts/'coupling/scalar_wake128.py',scripts/'coupling/scalar_gust128.py']
        if io_format or journal_mode:self.files += [scripts/'coupling/journal.py']
        self.pins = {str(p): hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
                     for p in self.files}
        self.binding = sealed({'configuration': config, 'pulse_digest': pulse['digest'],
                               'packet_steps': packet_steps, 'inputs': self.pins})
        if io_format or journal_mode:self.binding=sealed({**self.binding,'native_io_format':io_format,'journal_codec':self.codec.mode})
        if deposition_interval is not None:
            period = Fraction(deposition_interval)
            self.binding = sealed({**self.binding, 'deposition_interval':
                                   [period.numerator, period.denominator]})

    def unchanged(self):
        require(self.pins == {str(p): hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
                             for p in self.files}, 'replay implementation changed')
        require(self.binding['digest'] == digest(self.binding), 'replay binding mutated')

    def connection(self):
        db = sqlite3.connect(self.path, isolation_level=None)
        db.execute('PRAGMA synchronous=FULL')
        return db

    def inspect(self):
        self.unchanged()
        with closing(self.connection()) as db:
            require(db.execute('select binding from metadata').fetchone()[0] ==
                    self.binding['digest'], 'replay binding mismatch')
            revision, raw = db.execute('select revision,payload from checkpoints order by revision desc limit 1').fetchone()
            cp = self.codec.decode(raw)
        self.check(cp)
        require(cp['revision'] == revision, 'replay revision identity')
        return cp

    def check(self, cp):
        require(cp['digest'] == digest(cp) and cp['binding'] == self.binding['digest'],
                'replay checkpoint digest/binding')
        result = cp['fluid']
        require(result['digest'] == digest(result), 'replay native result digest')
        fluid_adapter.validate(self.config['fluid'], result['state'])
        require(result['state']['worker_sha256'] == self.pins[str(self.worker)],
                'replay native worker identity')
        fluid_adapter.conservation(self.config['fluid'], result)
        data = result['state']['data']
        # Native CFD persists steps * its configured double dt. Rational model
        # time still selects intervals, but a single rounded rational quotient
        # need not have the same bits as this native multiplication (e.g.1.4s).
        require(data['time_s'] == data['steps'] * self.config['fluid']['momentum_dt_s'],
                'replay exact native time')
        for key in ('energy_j', 'smoke_kg'):
            require(math.isclose(data['input_' + key], cp['delivered'][key],
                                 rel_tol=1e-12, abs_tol=1e-20), 'replay delivered input')

    def publish(self, result, parent, delivered):
        revision = 0 if parent is None else parent['revision'] + 1
        cp = sealed({'schema': 'physics_sim_transport_replay_checkpoint/v1',
                     'binding': self.binding['digest'], 'revision': revision,
                     'parent_digest': None if parent is None else parent['digest'],
                     'delivered': delivered, 'fluid': result})
        self.check(cp)
        raw = self.codec.encode(cp)
        require(self.path.stat().st_size + len(raw) < self.codec.limit,
                'replay journal resource limit')
        with closing(self.connection()) as db:
            db.execute('BEGIN IMMEDIATE')
            head = db.execute('select revision from checkpoints order by revision desc limit 1').fetchone()
            require(head == (None if parent is None else (parent['revision'],)),
                    'replay concurrent publication')
            db.execute('insert into checkpoints values(?,?)', (revision, raw))
            db.commit()
        retire_attempts(self.root / 'native-attempts', self.root / 'attempt-receipts')
        return cp

    def initialize(self):
        self.unchanged()
        require(not self.path.exists(), 'fresh replay journal')
        self.root.mkdir(parents=True, exist_ok=False)
        (self.root / 'binding.json').write_text(json.dumps(self.binding) + '\n')
        with closing(self.connection()) as db:
            db.execute('create table metadata(binding text not null)')
            db.execute('insert into metadata values(?)', (self.binding['digest'],))
            db.execute('create table checkpoints(revision integer primary key,payload blob not null)')
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(self.root / 'native-attempts')
        result = fluid_adapter.run(self.config['fluid'], None, [], self.worker,**({'io_format':self.io_format} if self.io_format else {}))
        return self.publish(result, None, {'energy_j': 0., 'smoke_kg': 0.})

    def advance(self, revision, count):
        parent = self.inspect()
        require(parent['revision'] == revision, 'expected replay revision')
        first = parent['fluid']['state']['data']['steps']
        steps = self.replay.steps(first, count, self.dt)
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(self.root / 'native-attempts')
        result = run(self.config['fluid'], parent['fluid']['state'], steps,
                     self.worker, self.packet_steps,**({'io_format':self.io_format} if self.io_format else {}))
        delivered = {key: math.fsum([parent['delivered'][key],
                                    *[math.fsum(s[key]) for s in steps]])
                     for key in ('energy_j', 'smoke_kg')}
        return self.publish(result, parent, delivered)

    def audit(self):
        self.unchanged()
        parent = None
        with closing(self.connection()) as db:
            require(db.execute('select binding from metadata').fetchone()[0] == self.binding['digest'],
                    'replay audit binding')
            for revision, raw in db.execute('select revision,payload from checkpoints order by revision'):
                cp = self.codec.decode(raw)
                self.check(cp)
                require(cp['revision'] == revision and cp['parent_digest'] ==
                        (None if parent is None else parent['digest']), 'replay audited chain')
                if parent is not None:
                    require(cp['fluid']['state']['data']['steps'] >
                            parent['fluid']['state']['data']['steps'], 'replay audited clock progress')
                parent = cp
        return parent
