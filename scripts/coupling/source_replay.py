"""Retained physical Fire pulses, independent of subsequent Fluid feedback.

The pulse keeps original producer frames and calibration. Overlap mapping uses
the ordinary receiver's clipped nodal dual areas and fixed physical depth.
No node values are rescaled to match a desired downstream temperature.
"""
from contextlib import closing
from fractions import Fraction
import copy
import hashlib
import math
from pathlib import Path
import sqlite3
import json

from numeric_digest_stream import digest, sealed
from surface_sources.growth_fire_v1 import frame_valid, require
from surface_sources.receiver import mapping
from .journal import Journal

SCHEMA = 'physics_sim_retained_fire_pulse/v1'
TICK_SCHEMA = 'physics_sim_retained_fire_tick_pulse/v1'
QUANTITIES = (('energy_j', 'energy_transferred_j'),
              ('smoke_kg', 'smoke_transferred_kg'))


def interval(frame):
    v = frame['interval']
    dt = Fraction(str(frame['config']['dt_s']))
    return v['start_tick'] * dt, v['end_tick'] * dt


def validate(pulse):
    require(pulse['schema'] in (SCHEMA, TICK_SCHEMA) and pulse['digest'] == digest(pulse),
            'retained pulse digest/schema')
    frames = pulse['frames']
    require(1 <= len(frames) <= (128 if pulse['schema'] == TICK_SCHEMA else 32),
            'bounded retained pulse')
    end = Fraction(0)
    for sequence, frame in enumerate(frames):
        frame_valid(frame)
        require(frame['config'] == frames[0]['config'] and
                frame['producer'] == frames[0]['producer'], 'pulse source identity')
        left, right = interval(frame)
        require(left == end and right > left, 'contiguous physical pulse clock')
        if pulse['schema'] == TICK_SCHEMA:
            require(frame['interval']['end_tick'] - frame['interval']['start_tick'] == 1
                    and frame['event']['sequence'] == sequence,
                    'native tick pulse clock/sequence')
        end = right
    require(pulse['end_s'] == float(end), 'pulse terminal clock')
    for _, quantity in QUANTITIES:
        total = math.fsum(f['totals'][quantity] for f in frames)
        require(total == pulse['totals'][quantity], 'pulse extensive total')
    return pulse


def extract(case):
    """Read only the accepted delivery prefix; leave the original journal intact."""
    case = Path(case).resolve()
    config = json.loads((case / 'config.json').read_text())
    summary = json.loads((case / 'summary.json').read_text())
    require(summary['status'] == 'completed' and summary['independent_reopen'],
            'completed independently reopened pulse donor')
    count = config['source_delivery']['active_windows']
    codec = Journal(config['journal_codec'])
    frames = []
    path = case / 'run/joint.sqlite3'
    with closing(sqlite3.connect(path.as_uri() + '?mode=ro', uri=True)) as db:
        parent = None
        for revision, payload in db.execute(
                'select revision,payload from checkpoints where revision<=? order by revision',
                (count,)):
            cp = codec.decode(payload)
            require(cp['digest'] == digest(cp) and cp['parent_digest'] == parent,
                    'pulse donor checkpoint chain')
            parent = cp['digest']
            if revision:
                frame = cp['source']['frame']
                receipt = cp['evidence']['source_delivery']
                require(receipt['active'] and receipt['delivered'] == receipt['available']
                        and receipt['source_frame_digest'] == frame['digest'],
                        'fully delivered donor pulse')
                frames.append(frame)
    require(len(frames) == count, 'complete donor prefix')
    pulse = sealed({'schema': SCHEMA, 'frames': frames,
                    'end_s': float(interval(frames[-1])[1]),
                    'totals': {q: math.fsum(f['totals'][q] for f in frames)
                               for _, q in QUANTITIES},
                    'donor': {'case': str(case), 'accepted_prefix_digest': parent,
                              'head_digest': summary['head_digest'],
                              'journal_sha256': hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()},
                    'feedback': 'recorded_donor_only_no_live_return'})
    return validate(pulse)


class Replay:
    """Allocate exact physical overlaps with sparse accumulation before densifying."""
    def __init__(self, pulse, receiver, *, deposition_interval=None):
        self.pulse = copy.deepcopy(validate(pulse))
        self.receiver = copy.deepcopy(receiver)
        self.deposition_interval = None if deposition_interval is None else Fraction(deposition_interval)
        self.count = math.prod(receiver['dimensions'])
        self.weights = []
        for frame in self.pulse['frames']:
            policy = {'schema': 'physics_sim_surface_source_policy/v1',
                      'consumer_id': 'retained-pulse-replay',
                      'model_id': 'fixed-physical-pulse',
                      'source_config': frame['config'], 'producer': frame['producer'],
                      'stream_start_tick': 0, 'receiver': receiver}
            self.weights.append(mapping(frame, policy))
        self._segments = [(interval(frame), frame['cells'], weights)
                          for frame, weights in zip(self.pulse['frames'], self.weights)]
        if self.deposition_interval is not None:
            from .source_timing import grouped_segments
            self._segments = grouped_segments(self.pulse, self.weights, self.deposition_interval)
        self.binding = digest(self._binding())

    def _binding(self):
        value = {'pulse': self.pulse, 'receiver': self.receiver}
        if self.deposition_interval is not None:
            value['deposition_interval'] = [self.deposition_interval.numerator,
                                            self.deposition_interval.denominator]
        return value

    def steps(self, first, count, dt):
        require(type(first) is int and first >= 0 and type(count) is int and
                1 <= count <= 256, 'bounded replay steps')
        dt = Fraction(dt)
        require(dt > 0 and dt <= 1, 'positive replay step')
        require(digest(self._binding()) == self.binding,
                'replay input mutated')
        steps = []
        for tick in range(first, first + count):
            start, end = tick * dt, (tick + 1) * dt
            terms = {key: {} for key, _ in QUANTITIES}
            for (left, right), cells, weights in self._segments:
                a, b = max(start, left), min(end, right)
                if b <= a:
                    continue
                lo, hi = float((a-left)/(right-left)), float((b-left)/(right-left))
                for node, row in enumerate(weights):
                    for key, source in QUANTITIES:
                        full = cells[source][node]
                        amount = (full if hi == 1 else full * hi) - full * lo
                        require(math.isfinite(amount) and amount >= 0,
                                'nonnegative pulse temporal overlap')
                        prior = []
                        for index, (cell, fraction) in enumerate(row):
                            value = amount - math.fsum(prior) if index == len(row)-1 else amount*fraction
                            require(value >= 0, 'pulse spatial remainder')
                            prior.append(value)
                            if value:
                                terms[key].setdefault(cell, []).append(value)
            fields = {key: [0.] * self.count for key, _ in QUANTITIES}
            for key, cells in terms.items():
                for cell, values in cells.items():
                    fields[key][cell] = math.fsum(values)
            steps.append(fields)
        return steps
