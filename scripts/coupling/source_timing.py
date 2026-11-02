"""Controlled emission timing and explicit regrouping of immutable producer frames.

Regrouping is a deposition approximation. It never rewrites producer event
identity or pretends that grouped amounts were emitted at a finer time.
"""
from contextlib import closing
from fractions import Fraction
import base64
import hashlib
import json
import math
from pathlib import Path
import sqlite3

from numeric_digest_stream import digest, sealed
from surface_sources.growth_fire_v1 import require
from .journal import Journal
from .source_replay import interval, validate, TICK_SCHEMA, QUANTITIES


def grouped_segments(pulse, weights, period):
    validate(pulse)
    period = Fraction(period)
    end = interval(pulse['frames'][-1])[1]
    require(period > 0 and period <= end and (end / period).denominator == 1,
            'deposition period divides pulse horizon')
    groups = [[] for _ in range(int(end / period))]
    for frame, mapping in zip(pulse['frames'], weights):
        a, b = interval(frame)
        index = int(a / period)
        require(b <= (index + 1) * period, 'deposition group cannot cut a producer frame')
        groups[index].append((frame, mapping))
    segments = []
    for index, items in enumerate(groups):
        require(items and interval(items[0][0])[0] == index * period and
                interval(items[-1][0])[1] == (index + 1) * period,
                'complete deposition group')
        mapping = items[0][1]
        require(all(w == mapping for _, w in items), 'fixed physical source mapping')
        cells = {quantity: [math.fsum(f['cells'][quantity][node] for f, _ in items)
                            for node in range(len(items[0][0]['cells'][quantity]))]
                 for _, quantity in QUANTITIES}
        segments.append(((index * period, (index + 1) * period), cells, mapping))
    return segments


def native_tick_pulse(case, *, branch='native-tick-timing', progress=None):
    """Derive one Fire-tick frames on an explicit fork with donor wind held.

    Every donor endpoint must have exactly identical native state bytes. This
    changes temporal observation/deposition, not native Fire evolution or gain.
    """
    from .framework import load_fire
    case = Path(case).resolve()
    config = json.loads((case / 'config.json').read_text())
    summary = json.loads((case / 'summary.json').read_text())
    require(summary['status'] == 'completed' and summary['independent_reopen'],
            'completed native timing donor')
    codec = Journal(config['journal_codec'])
    count = config['source_delivery']['active_windows']
    with closing(sqlite3.connect((case/'run/joint.sqlite3').as_uri()+'?mode=ro', uri=True)) as db:
        encoded = list(db.execute('select revision,payload from checkpoints where revision<=? order by revision', (count,)))
    fire = load_fire(config['growth_root'])
    parent = None
    donor_digest = None
    frames, checks = [], []
    for revision, raw in encoded:
        cp = codec.decode(raw)
        require(cp['revision'] == revision and digest(cp) == cp['digest'] and
                cp['parent_digest'] == donor_digest, 'native timing donor chain')
        donor_digest = cp['digest']
        if revision == 0:
            parent = fire.fork(cp['source'], branch, 'native-timing-fork')
            continue
        original = cp['source']['frame']
        ticks = original['interval']['end_tick'] - original['interval']['start_tick']
        require(len(frames) + ticks <= 128, 'native timing frame capacity')
        wind = cp['evidence']['held_return']['wind_input']
        begin = len(frames)
        for _ in range(ticks):
            parent = fire.advance_exact(parent, config['fire_worker'], 1,
                                        'timing-tick-' + str(len(frames)),
                                        config['periods'][0], wind=wind)
            frames.append(parent['frame'])
        actual = base64.b64decode(parent['checkpoint']['native_state_b64'])
        expected = base64.b64decode(cp['source']['checkpoint']['native_state_b64'])
        require(actual == expected, 'native Fire timing endpoint mismatch')
        for _, quantity in QUANTITIES:
            for node, target in enumerate(original['cells'][quantity]):
                amount = math.fsum(f['cells'][quantity][node] for f in frames[begin:])
                require(math.isclose(amount, target, rel_tol=1e-12, abs_tol=1e-20),
                        'native emission spatial/fuel calibration partition')
        checks.append({'donor_revision': revision, 'native_end_sha256':
                       hashlib.sha256(actual).hexdigest(), 'native_bytes_exact': True})
        if progress is not None:
            progress(revision, len(frames))
    require(len(checks) == count and frames, 'complete native emission prefix')
    pulse = sealed({'schema': TICK_SCHEMA, 'frames': frames,
                    'end_s': float(interval(frames[-1])[1]),
                    'totals': {q: math.fsum(f['totals'][q] for f in frames)
                               for _, q in QUANTITIES},
                    'donor': {'case': str(case), 'accepted_prefix_digest': donor_digest,
                              'head_digest': summary['head_digest'],
                              'derived_branch': branch},
                    'feedback': 'recorded_donor_only_no_live_return'})
    return validate(pulse), {'schema': 'native_fire_tick_partition_proof/v1',
                             'windows': checks, 'final_bundle': parent}
