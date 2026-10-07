"""Offline source admission and allocation plans; never executes a fluid solver."""
import json
from contextlib import closing
import math
from pathlib import Path
import sqlite3
from .growth_fire_v1 import (FRAME, BUNDLE, canonical, sealed, digest,
    frame_valid, bundle_valid, config_valid, keys, require, number, integer,
    identifier, hash_valid)

POLICY = 'physics_sim_surface_source_policy/v1'
TRANSFERS = ('energy_transferred_j', 'smoke_transferred_kg')
MAX_ENTRIES = 262144
MAX_EVENTS = 256
MAX_PLANS = 4096
MAX_JOURNAL_BYTES = 128*1024*1024


def policy_valid(p):
    keys(p, ('schema', 'consumer_id', 'model_id', 'source_config', 'producer',
             'stream_start_tick', 'receiver'))
    require(p['schema'] == POLICY, 'unsupported receiver policy')
    identifier(p['consumer_id']); identifier(p['model_id'])
    config_valid(p['source_config'])
    keys(p['producer'], ('worker_sha256', 'adapter_sha256'))
    for h in p['producer'].values(): hash_valid(h)
    integer(p['stream_start_tick'], 0, 2**32-1)
    g = p['receiver']
    support = 'injection_depth_m' in g
    keys(g, ('frame', 'origin_m', 'spacing_m', 'dimensions', 'layer', 'fluid_mask',
             *(('injection_depth_m', 'injection_fluid_mask') if support else ())))
    require(g['frame'] == 'right_handed_z_up_meters', 'receiver coordinate frame')
    for key in ('origin_m', 'spacing_m', 'dimensions'):
        require(type(g[key]) is list and len(g[key]) == 3, 'receiver vector dimensions')
    for v in g['origin_m']: number(v, -1e9, 1e9)
    for v in g['spacing_m']: number(v, 1e-6, 1e6)
    for v in g['dimensions']: integer(v, 1, 256)
    nx, ny, nz = g['dimensions']
    require(nx*ny*nz <= 524288, 'receiver cell admission budget')
    integer(g['layer'], 0, nz-1)
    require(type(g['fluid_mask']) is list and len(g['fluid_mask']) == nx*ny,
            'receiver layer mask length')
    require(all(type(v) is int and v in (0,1) for v in g['fluid_mask']), 'receiver fluid mask')
    if support:
        number(g['injection_depth_m'], 1e-6, (nz-g['layer'])*g['spacing_m'][2])
        layers = math.ceil(g['injection_depth_m']/g['spacing_m'][2])
        mask = g['injection_fluid_mask']
        require(type(mask) is list and len(mask) == layers*nx*ny,
                'physical injection mask length')
        require(all(type(v) is int and v in (0,1) for v in mask), 'physical injection mask')
        require(mask[:nx*ny] == g['fluid_mask'], 'physical injection base mask identity')
    return p


def source_frame(value):
    require(type(value) is dict, 'source object required')
    if value.get('schema') == BUNDLE:
        bundle_valid(value)
        require(value['frame'] is not None, 'initial/fork bundle has no source interval')
        return value['frame']
    return frame_valid(value)


def mapping(frame, policy):
    """Area intersections of clipped nodal dual rectangles with XY receiving cells."""
    policy_valid(policy)
    c, g = frame['config'], policy['receiver']
    ox, oy, oz = g['origin_m']; dx, dy, dz = g['spacing_m']
    nx, ny, _ = g['dimensions']; layer = g['layer']
    sx, sy, sz = c['origin_m']; h = c['cell_size_m']
    ex, ey = sx+(c['width']-1)*h, sy+(c['height']-1)*h
    require(sz == oz+layer*dz, 'source must lie on declared layer lower XY face')
    require(ox <= sx < ex <= ox+nx*dx and oy <= sy < ey <= oy+ny*dy,
            'source patch outside receiving layer')
    rows = []; entries = 0
    for y in range(c['height']):
        for x in range(c['width']):
            ax, bx = sx+max(0,x-.5)*h, sx+min(c['width']-1,x+.5)*h
            ay, by = sy+max(0,y-.5)*h, sy+min(c['height']-1,y+.5)*h
            weights = []
            # Include a boundary neighbor, then exclude exactly zero intersections.
            for j in range(max(0,math.floor((ay-oy)/dy)),min(ny,math.floor((by-oy)/dy)+1)):
                for i in range(max(0,math.floor((ax-ox)/dx)),min(nx,math.floor((bx-ox)/dx)+1)):
                    area = max(0,min(bx,ox+(i+1)*dx)-max(ax,ox+i*dx))*max(0,min(by,oy+(j+1)*dy)-max(ay,oy+j*dy))
                    if area > 0:
                        require(g['fluid_mask'][j*nx+i] == 1, 'source overlaps declared solid receiver')
                        weights.append(((layer*ny+j)*nx+i,area))
            total = math.fsum(a for _,a in weights)
            area = frame['surface']['area_m2'][y*c['width']+x]
            require(weights and math.isclose(total,area,rel_tol=1e-12,abs_tol=area*1e-12),
                    'surface coverage cannot be established at receiver precision')
            fractions = [a/total for _,a in weights]
            fractions[-1] = 1-math.fsum(fractions[:-1])
            require(all(v > 0 for v in fractions), 'unresolved mapping precision')
            row = [(cell,fraction) for (cell,_),fraction in zip(weights,fractions)]
            if 'injection_depth_m' in g:
                depth = g['injection_depth_m']; expanded = []
                for offset in range(math.ceil(depth/dz)):
                    overlap = min(dz, depth-offset*dz)
                    for cell,fraction in row:
                        xy = cell % (nx*ny)
                        require(g['injection_fluid_mask'][offset*nx*ny+xy] == 1,
                                'physical injection overlaps declared solid')
                        expanded.append((cell+offset*nx*ny, fraction*overlap/depth))
                remainder = 1-math.fsum(fraction for _,fraction in expanded[:-1])
                require(remainder > 0, 'physical injection precision')
                expanded[-1] = (expanded[-1][0], remainder)
                row = expanded
            rows.append(row)
            entries += len(row)
            require(entries <= MAX_ENTRIES, 'surface mapping resource limit')
    return rows


def allocate(frame, policy, boundaries, plan_id):
    return _allocate_mapped(frame,policy,boundaries,plan_id,mapping(frame,policy))


def _allocate_mapped(frame, policy, boundaries, plan_id, weights):
    """Internal adapter path: mapping prepared from its bound geometry policy."""
    identifier(plan_id)
    require(type(boundaries) is list and 2 <= len(boundaries) <= 257, '2..257 substep boundaries required')
    for t in boundaries: number(t,0)
    start,end = frame['interval']['start_s'],frame['interval']['end_s']
    require(start <= boundaries[0] < boundaries[-1] <= end and
            all(a < b for a,b in zip(boundaries,boundaries[1:])), 'ordered substeps inside half-open interval required')
    steps = []; count = 0
    for index,(a,b) in enumerate(zip(boundaries,boundaries[1:])):
        values = {}
        for node,row in enumerate(weights):
            for quantity in TRANSFERS:
                full = frame['cells'][quantity][node]
                # Cumulative differences make adjacent interval slices telescope.
                left = full*((a-start)/(end-start))
                right = full if b == end else full*((b-start)/(end-start))
                amount = right-left
                require(amount >= 0 and math.isfinite(amount), 'allocation precision')
                prior = []
                for j,(cell,fraction) in enumerate(row):
                    part = amount-math.fsum(prior) if j == len(row)-1 else amount*fraction
                    require(part >= 0, 'negative allocation remainder')
                    prior.append(part)
                    if part:
                        values.setdefault(cell,{k:[] for k in TRANSFERS})[quantity].append(part)
        cells = [{'cell_index':cell, **{k:math.fsum(v[k]) for k in TRANSFERS}}
                 for cell,v in sorted(values.items())]
        count += len(cells); require(count <= MAX_ENTRIES, 'allocation receipt resource limit')
        steps.append({'start_s':a,'end_s':b,'cells':cells,
                      'totals':{k:math.fsum(c[k] for c in cells) for k in TRANSFERS}})
    budgets = {}
    for k in TRANSFERS:
        full = frame['totals'][k]
        before = full*((boundaries[0]-start)/(end-start))
        after = full*((end-boundaries[-1])/(end-start))
        planned = math.fsum(s['totals'][k] for s in steps)
        require(math.isclose(full,before+planned+after,rel_tol=1e-12,abs_tol=1e-12), 'allocation budget')
        budgets[k]={'source':full,'retained_before':before,'planned':planned,'remaining_after':after}
    return sealed({'schema':'physics_sim_surface_allocation_plan/v1','plan_id':plan_id,
        'source_digest':frame['digest'],'event':frame['event'],'policy_digest':digest(policy),
        'consumer_id':policy['consumer_id'],'model_id':policy['model_id'],
        'receiver':policy['receiver'],'steps':steps,'budgets':budgets,
        'status':'allocation_plan_only','solver_mutated':False,'physically_applied':False})


class Receiver:
    """One explicit source branch and immutable receiver policy per SQLite journal."""
    def __init__(self, store, policy):
        self.policy = json.loads(canonical(policy_valid(policy)))
        self.path = Path(store)/'surface_sources.sqlite3'
        self.policy_digest = digest(self.policy)

    def _connection(self):
        self.path.parent.mkdir(parents=True,exist_ok=True)
        db = sqlite3.connect(self.path,timeout=10,isolation_level=None)
        db.execute('PRAGMA synchronous=FULL')
        return db

    def admit(self, value):
        frame = json.loads(canonical(source_frame(value)))
        require(digest(frame['config']) == digest(self.policy['source_config']), 'source config/calibration/lineage differs from policy')
        require(frame['producer'] == self.policy['producer'], 'source provenance differs from policy')
        require(digest(self.policy) == self.policy_digest, "receiver policy mutated")
        mapping(frame,self.policy)  # all geometry admission before journal creation
        if not self.path.exists():
            require(frame["event"]["sequence"] == 0 and frame["interval"]["start_tick"] == self.policy["stream_start_tick"], "out-of-order first interval")
        with closing(self._connection()) as db:
            try:
                db.execute('BEGIN IMMEDIATE')
                db.execute('CREATE TABLE IF NOT EXISTS metadata (digest TEXT NOT NULL)')
                db.execute('CREATE TABLE IF NOT EXISTS events (sequence INTEGER PRIMARY KEY, digest TEXT NOT NULL, frame TEXT NOT NULL, receipt TEXT NOT NULL)')
                db.execute('CREATE TABLE IF NOT EXISTS plans (id TEXT PRIMARY KEY, digest TEXT NOT NULL, receipt TEXT NOT NULL)')
                bound = db.execute('SELECT digest FROM metadata').fetchone()
                require(bound is None or bound[0] == self.policy_digest, 'journal bound to another policy')
                seq = frame['event']['sequence']
                old = db.execute('SELECT digest,receipt FROM events WHERE sequence=?',(seq,)).fetchone()
                if old:
                    require(old[0] == frame['digest'], 'event content conflict')
                    db.rollback(); return {'status':'replayed','receipt':json.loads(old[1])}
                require(db.execute('SELECT count(*) FROM events').fetchone()[0] < MAX_EVENTS, 'event journal capacity')
                require(self.path.stat().st_size + 2*(len(canonical(frame))+4096) <= MAX_JOURNAL_BYTES, 'journal byte budget')
                previous = db.execute('SELECT sequence,frame FROM events ORDER BY sequence DESC LIMIT 1').fetchone()
                if previous:
                    prior = json.loads(previous[1]); next_seq=previous[0]+1; next_tick=prior['interval']['end_tick']
                else: next_seq=0; next_tick=self.policy['stream_start_tick']
                require(seq == next_seq and frame['interval']['start_tick'] == next_tick,'out-of-order source interval')
                receipt=sealed({'schema':'physics_sim_surface_admission/v1','event':frame['event'],
                    'source_digest':frame['digest'],'policy_digest':self.policy_digest,
                    'interval':frame['interval'],'source_totals':frame['totals'],
                    'status':'admitted_for_offline_planning','solver_mutated':False,'physically_applied':False})
                if bound is None: db.execute('INSERT INTO metadata VALUES (?)',(self.policy_digest,))
                db.execute('INSERT INTO events VALUES (?,?,?,?)',(seq,frame['digest'],canonical(frame).decode(),canonical(receipt).decode()))
                db.commit(); return {'status':'admitted','receipt':receipt}
            except BaseException:
                db.rollback(); raise

    def plan(self, sequence, boundaries, plan_id):
        integer(sequence,0,2**32-1); identifier(plan_id)
        require(digest(self.policy) == self.policy_digest, 'receiver policy mutated')
        require(self.path.is_file(),'no admitted source journal')
        with closing(self._connection()) as db:
            try:
                db.execute('BEGIN IMMEDIATE')
                bound = db.execute('SELECT digest FROM metadata').fetchone()
                require(bound is not None and bound[0] == self.policy_digest, 'journal policy mismatch')
                row=db.execute('SELECT frame FROM events WHERE sequence=?',(sequence,)).fetchone()
                require(row is not None,'source event not admitted')
                receipt=allocate(json.loads(row[0]),self.policy,boundaries,plan_id)
                old=db.execute('SELECT digest,receipt FROM plans WHERE id=?',(plan_id,)).fetchone()
                if old:
                    require(old[0] == receipt['digest'],'plan ID content conflict')
                    db.rollback(); return {'status':'replayed','receipt':json.loads(old[1])}
                require(db.execute('SELECT count(*) FROM plans').fetchone()[0] < MAX_PLANS, 'plan journal capacity')
                require(self.path.stat().st_size + 2*(len(canonical(receipt))+4096) <= MAX_JOURNAL_BYTES, 'journal byte budget')
                db.execute('INSERT INTO plans VALUES (?,?,?)',(plan_id,receipt['digest'],canonical(receipt).decode()))
                db.commit(); return {'status':'planned','receipt':receipt}
            except BaseException:
                db.rollback(); raise

    def inspect(self):
        if not self.path.exists(): return {'admitted_events':0,'allocation_plans':0,'physically_applied':False,'solver_mutated':False}
        db=sqlite3.connect(self.path.resolve().as_uri()+'?mode=ro',uri=True)
        try:
            if not db.execute("SELECT name FROM sqlite_master WHERE name='metadata'").fetchone():
                return {'admitted_events':0,'allocation_plans':0,'physically_applied':False,'solver_mutated':False}
            bound = db.execute('SELECT digest FROM metadata').fetchone()
            require(bound is not None and bound[0] == self.policy_digest,'journal policy mismatch')
            last=db.execute('SELECT sequence,frame FROM events ORDER BY sequence DESC LIMIT 1').fetchone()
            return {'policy_digest':self.policy_digest,
                'admitted_events':db.execute('SELECT count(*) FROM events').fetchone()[0],
                'allocation_plans':db.execute('SELECT count(*) FROM plans').fetchone()[0],
                'next_sequence':last[0]+1 if last else 0,
                'next_tick':json.loads(last[1])['interval']['end_tick'] if last else self.policy['stream_start_tick'],
                'physically_applied':False,'solver_mutated':False}
        finally: db.close()
