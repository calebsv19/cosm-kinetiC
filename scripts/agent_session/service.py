"""Trusted-local session service shared by MCP, CLI and desktop clients."""
from contextlib import contextmanager
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
TERMINAL = {'completed', 'cancelled', 'failed'}


class SessionError(ValueError):
    pass


def encode(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)


def digest(value):
    return hashlib.sha256(encode(value).encode()).hexdigest()


def atomic(path, value):
    path = Path(path)
    fd, tmp = tempfile.mkstemp(prefix='.' + path.name, dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as f:
            f.write(encode(value) + '\n')
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def read(path):
    return json.loads(Path(path).read_text())


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', value):
        raise SessionError('ID must contain 1-64 letters, digits, underscores or hyphens')
    return value


def number(value, low, high, integral=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise SessionError('expected finite number')
    if not low <= value <= high or (integral and not isinstance(value, int)):
        raise SessionError(f'expected {"integer" if integral else "number"} in [{low}, {high}]')
    return value


class Service:
    def __init__(self, root=None, worker=None):
        self.root = Path(root or os.environ.get('PHYSICS_SIM_SESSION_ROOT', REPO / 'data/runtime/agent_sessions')).resolve()
        self.worker = Path(worker or os.environ.get('PHYSICS_SIM_SESSION_WORKER', REPO / 'physics_sim_session_worker')).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        for name in ('scenes', 'runs'):
            (self.root / name).mkdir(exist_ok=True)
        self.children = {}

    @contextmanager
    def lock(self):
        with (self.root / 'service.lock').open('a') as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            yield

    def run_dir(self, run_id):
        path = self.root / 'runs' / identifier(run_id)
        if not (path / 'request.json').exists():
            raise SessionError('unknown run_id')
        return path

    def capabilities(self):
        return {'schema': 'physics_sim_session_capabilities_v1', 'models': ['wind_approximate_v1'],
                'templates': ['wind_box', 'wind_sphere'], 'active_runs_per_root': 1,
                'controls': ['pause', 'step', 'continue', 'cancel'], 'checkpoint_restart': False,
                'inspection': {'planes':['XY','XZ','YZ'],'fields':['speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity'], 'max_samples':4096,'max_probes':16,'history_points':128,'pending_requests':8,'retained_requests':32},
                'states': ['starting','running','paused','completed','cancelled','failed'],
                'tick_semantics': 'one fixed dt, including configured core_sim substeps; acknowledgement at safe boundaries',
                'preview': 'XY midpoint, at most 64x64 sparse samples; [speed,dye_density,solid]',
                'model_limitations': 'Injected wake and pressure/drag proxies; not validated predictive CFD',
                'root': str(self.root), 'worker_available': self.worker.is_file()}

    def scene_create(self, scene_id, template='wind_box', dimensions=None, inflow_speed=2.0):
        scene_id = identifier(scene_id)
        if template not in ('wind_box', 'wind_sphere'):
            raise SessionError('unsupported scene template')
        dimensions = dimensions if dimensions is not None else [2.0, 1.0, 1.0]
        if not isinstance(dimensions, list) or len(dimensions) != 3:
            raise SessionError('dimensions must contain three lengths in meters')
        lengths = [number(x, 0.1, 100) for x in dimensions]
        speed = number(inflow_speed, 0.01, 100)
        # This is a bounded authoring template, not an arbitrary runtime-JSON editor.
        doc = {'schema_family': 'codework_scene', 'schema_variant': 'scene_runtime_v1',
               'schema_version': 1, 'scene_id': scene_id, 'space_mode_default': '3d',
               'unit_system': 'meters', 'world_scale': 1.0,
               'objects': [{'object_id': 'obstacle', 'object_type': 'box' if template == 'wind_box' else 'circle',
                            'transform': {'position': dict(zip('xyz', [lengths[0]*.45, lengths[1]*.5, lengths[2]*.5])),
                                          'scale': dict(zip('xyz', [min(lengths)*.18]*3)),
                                          'rotation': dict.fromkeys('xyz', 0.0)}, 'flags': {'locked': True}}],
               'materials': [], 'lights': [], 'cameras': [], 'constraints': [],
               'extensions': {'physics_sim': {
                   'scene_domain': {'active': True, 'shape': 'box', 'min': dict.fromkeys('xyz', 0.0),
                                    'max': dict(zip('xyz', lengths))},
                   'wind_tunnel': {'active': True, 'inlet_face': 'left', 'outlet_face': 'right',
                                   'inflow_speed': speed, 'inflow_density': .75, 'inlet_slab_cells': 2,
                                   'outlet_policy': 'receive', 'wall_policy': 'no_slip'}}}}
        template_fingerprint = digest(doc)
        path = self.root / 'scenes' / scene_id
        with self.lock():
            if path.exists():
                manifest = read(path / 'scene.json')
                if manifest.get('template_fingerprint',manifest['revision']) != template_fingerprint:
                    raise SessionError('scene_id already exists with different parameters; use a new ID')
                revision = manifest['revision']
            else:
                with tempfile.TemporaryDirectory(prefix='author-',dir=self.root / 'scenes') as tmp:
                    stage = Path(tmp)
                    atomic(stage / 'scene_authoring.json', dict(doc, schema_variant='scene_authoring_v1'))
                    compiled = subprocess.run([str(self.worker),'--compile',str(stage / 'scene_authoring.json'),
                                               str(stage / 'scene_runtime.json')], capture_output=True,text=True,timeout=30)
                    if compiled.returncode:
                        raise SessionError('scene_compile_failed: '+compiled.stderr[-1000:])
                    revision = digest(read(stage / 'scene_runtime.json'))
                    atomic(stage / 'scene.json', {'scene_id':scene_id,'revision':revision,'template':template,
                                                 'template_fingerprint':template_fingerprint})
                    os.replace(stage,path)
        return {'scene_id': scene_id, 'scene_revision': revision, 'project_path': str(path)}

    def request(self, scene_id, scene_revision, grid=None, steps=100, dt=.0166666667,
                start_paused=True, solver_cell_budget=0):
        scene = self.root / 'scenes' / identifier(scene_id)
        if not (scene / 'scene_runtime.json').exists():
            raise SessionError('unknown scene_id')
        doc = read(scene / 'scene_runtime.json')
        if digest(doc) != scene_revision or read(scene / 'scene.json')['revision'] != scene_revision:
            raise SessionError('stale_scene_revision')
        grid = grid if grid is not None else [32, 16, 16]
        if not isinstance(grid, list) or len(grid) != 3:
            raise SessionError('grid must contain three integers')
        grid = [number(x, 4, 256, True) for x in grid]
        if not isinstance(start_paused, bool):
            raise SessionError('start_paused must be boolean')
        req = {'schema': 'physics_sim_session_request_v1', 'scene_id': scene_id,
               'scene_revision': scene_revision, 'grid': grid, 'steps': number(steps, 1, 100000, True),
               'dt': number(dt, .00001, .1), 'start_paused': start_paused,
               'solver_cell_budget': number(solver_cell_budget, 0, 16777216, True)}
        return req, doc

    def scene_validate(self, scene_id, scene_revision, grid=None):
        req, doc = self.request(scene_id, scene_revision, grid)
        with tempfile.TemporaryDirectory(prefix='validate-', dir=self.root) as tmp:
            root = Path(tmp)
            atomic(root / 'request.json', dict(req, run_id='validation'))
            atomic(root / 'scene_runtime.json', doc)
            result = subprocess.run([str(self.worker), str(root), '--validate'], capture_output=True,
                                    text=True, timeout=30)
            if result.returncode:
                raise SessionError('scene_validation_failed: ' + result.stderr[-1000:])
            status = json.loads(result.stdout)
            return {k: status[k] for k in ('effective_grid', 'voxel_size_m', 'estimated_dense_bytes', 'model_limitations')} | {
                'valid': True, 'scene_revision': scene_revision, 'requested_grid': req['grid'],
                'resolution_changed': req['grid'] != status['effective_grid']}

    @staticmethod
    def owner_alive(path):
        with (path / 'owner.lock').open('a') as f:
            try:
                fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return True
            return False

    def _status(self, path):
        req = read(path / 'request.json')
        child = self.children.get(req['run_id'])
        if child:
            child.poll()  # Reap children owned by this service instance.
        snap_path = path / 'snapshot.json'
        snap = read(snap_path) if snap_path.exists() else {
            'schema': 'physics_sim_session_snapshot_v1', 'run_id': req['run_id'],
            'scene_revision': req['scene_revision'], 'state': 'starting', 'tick': 0, 'sequence': 0}
        if snap['state'] not in TERMINAL and not self.owner_alive(path):
            snap = dict(snap, state='failed', error='worker_exited_without_terminal_result',
                        sequence=snap['sequence']+1, updated_at=time.time())
            atomic(snap_path, snap)
            with (path / 'events.jsonl').open('a') as events:
                events.write(encode({'cursor':snap['sequence'],'tick':snap['tick'],
                                     'state':'failed','error':snap['error']})+'\n')
        return snap

    def run_start(self, request_id, scene_id, scene_revision, **options):
        run_id = identifier(request_id)
        req, doc = self.request(scene_id, scene_revision, **options)
        fingerprint = digest(req)
        with self.lock():
            path = self.root / 'runs' / run_id
            if len(str(path / 'scene_runtime.json').encode()) >= 512:
                raise SessionError('session root path exceeds solver path capacity')
            if path.exists():
                old = read(path / 'request.json')
                if old['request_fingerprint'] != fingerprint:
                    raise SessionError('request_id_conflict')
                return self._compact(self._status(path))
            for other in (self.root / 'runs').iterdir():
                if (other / 'request.json').exists() and self._status(other)['state'] not in TERMINAL:
                    raise SessionError('active_run_exists')
            if not self.worker.is_file():
                raise SessionError('build physics_sim_session_worker first')
            req.update(run_id=run_id, request_fingerprint=fingerprint,
                       worker_sha256=hashlib.sha256(self.worker.read_bytes()).hexdigest(),
                       created_at=time.time(), worker_path=str(self.worker))
            path.mkdir()
            for name in ('commands', 'receipts', 'command_ids'):
                (path / name).mkdir()
            atomic(path / 'request.json', req)
            atomic(path / 'scene_runtime.json', doc)
            with (path / 'worker.log').open('wb') as log, (path / 'owner.lock').open('a') as owner:
                fcntl.flock(owner, fcntl.LOCK_EX)
                env = dict(os.environ, PHYSICS_SIM_SESSION_OWNER_FD=str(owner.fileno()))
                child = subprocess.Popen([str(self.worker), str(path)], stdout=log, stderr=log,
                                         start_new_session=True, pass_fds=(owner.fileno(),), env=env)
            self.children[run_id] = child
            deadline = time.monotonic()+5
            while not (path / 'ready').exists() and child.poll() is None and time.monotonic()<deadline:
                time.sleep(.005)
            if not (path / 'ready').exists() and child.poll() is None:
                # Preserve ownership through uncertain startup; never launch another worker.
                atomic(path / 'snapshot.json', {'state': 'starting', 'run_id': run_id,
                       'scene_revision': scene_revision, 'tick': 0, 'sequence': 0, 'startup_pid': child.pid})
                raise SessionError('worker_startup_unconfirmed; inspect the same run')
            atomic(self.root / 'active.json', {'run_id': run_id, 'run_dir': str(path)})
            return self._compact(self._status(path))

    @staticmethod
    def _compact(snap):
        return {k: v for k, v in snap.items() if k not in ('preview','history')}

    def run_inspect(self, run_id, preview=False, log_tail_lines=0, history=False):
        number(log_tail_lines,0,100,True)
        with self.lock():
            path = self.run_dir(run_id)
            snap = self._status(path)
            snap['worker_alive'] = self.owner_alive(path)
            snap['snapshot_age_seconds'] = max(0,time.time()-snap.get('updated_at',time.time()))
        if log_tail_lines:
            with (path / 'worker.log').open('rb') as log:
                log.seek(0,2)
                size = log.tell()
                log.seek(max(0,size-65536))
                snap['log_tail'] = log.read().decode('utf-8',errors='replace').splitlines()[-log_tail_lines:]
        result = snap if preview else self._compact(snap)
        if history: result['history'] = snap.get('history',[])
        elif preview: result.pop('history',None)
        return result

    def run_control(self, run_id, command_id, action, scene_revision, wait_ms=0):
        identifier(command_id)
        if action not in ('pause', 'step', 'continue', 'cancel'):
            raise SessionError('unsupported action')
        number(wait_ms, 0, 5000, True)
        with self.lock():
            path = self.run_dir(run_id)
            req = read(path / 'request.json')
            if req['scene_revision'] != scene_revision:
                raise SessionError('stale_scene_revision')
            # Recover an interrupted queue publication before allocating another sequence.
            for saved in (path / 'command_ids').glob('*.json'):
                entry = read(saved)
                queued = path / 'commands' / f"{entry['sequence']:08d}.json"
                if not queued.exists():
                    atomic(queued, entry['payload'])
            payload = {'command_id': command_id, 'action': action, 'scene_revision': scene_revision}
            identity = path / 'command_ids' / (command_id+'.json')
            if identity.exists():
                record = read(identity)
                if record['payload'] != payload:
                    raise SessionError('command_id_conflict')
                seq = record['sequence']
            else:
                if self._status(path)['state'] in TERMINAL:
                    raise SessionError('run_is_terminal')
                seq = 1 + max([int(p.stem) for p in (path / 'commands').glob('*.json')], default=0)
                # Durable identity precedes publication; replay repairs an interrupted publish.
                atomic(identity, {'payload': payload, 'sequence': seq})
            command_path = path / 'commands' / f'{seq:08d}.json'
            if not command_path.exists():
                atomic(command_path, payload)
        receipt = path / 'receipts' / f'{seq:08d}.json'
        deadline = time.monotonic()+wait_ms/1000
        while True:
            if receipt.exists():
                return read(receipt)
            if time.monotonic() >= deadline:
                snap = self.run_inspect(run_id)
                return {'command_id': command_id, 'sequence': seq,
                        'status': 'not_applied' if snap['state'] in TERMINAL else 'pending',
                        'run_state': snap['state']}
            time.sleep(.01)

    def run_events(self, run_id, after_cursor=0, wait_ms=0):
        number(after_cursor, 0, 2**53, True); number(wait_ms, 0, 5000, True)
        path = self.run_dir(run_id) / 'events.jsonl'
        deadline = time.monotonic()+wait_ms/1000
        while True:
            events = []
            if path.exists():
                with path.open() as f:
                    for line in f:
                        try:
                            event = json.loads(line)
                        except json.JSONDecodeError:
                            continue  # An append in progress is retried on the next call.
                        if event['cursor'] > after_cursor:
                            events.append(event)
                            if len(events) == 100:
                                break
            status = self.run_inspect(run_id)
            if events or status['state'] in TERMINAL or time.monotonic() >= deadline:
                return {'events': events, 'next_cursor': events[-1]['cursor'] if events else after_cursor,
                        'status': status}
            time.sleep(.05)

    def run_result(self, run_id):
        status = self.run_inspect(run_id)
        if status['state'] not in TERMINAL:
            raise SessionError('run_not_terminal')
        path = self.run_dir(run_id)
        deadline = time.monotonic()+1
        while self.owner_alive(path) and time.monotonic()<deadline:
            time.sleep(.01)
        if self.owner_alive(path):
            raise SessionError('terminal_cleanup_pending; retry this result')
        artifacts = []
        files = [path / n for n in ('request.json', 'scene_runtime.json', 'snapshot.json', 'events.jsonl', 'worker.log')]
        files.extend(sorted((path / 'output').rglob('*')))
        for file in files:
            if file.is_file():
                artifacts.append({'path': str(file), 'bytes': file.stat().st_size,
                                  'sha256': hashlib.sha256(file.read_bytes()).hexdigest()})
        result = {'status': status, 'provenance': read(path / 'request.json'), 'artifacts': artifacts}
        atomic(path / 'result.json', result)
        return result

    def run_sample(self, run_id, request_id, plane='XY', position=.5, resolution=48,
                   field='speed', points=None, wait_ms=0, color_range=None, vectors=False):
        identifier(request_id)
        if plane not in ('XY','XZ','YZ'): raise SessionError('plane must be XY, XZ or YZ')
        fields=('speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity')
        if field not in fields: raise SessionError('unsupported field')
        number(position,0,1);number(resolution,4,64,True);number(wait_ms,0,5000,True)
        if not isinstance(vectors,bool): raise SessionError('vectors must be boolean')
        points=[] if points is None else points
        if not isinstance(points,list) or len(points)>16: raise SessionError('at most 16 world-space probes')
        for point in points:
            if not isinstance(point,list) or len(point)!=3: raise SessionError('each probe needs XYZ meters')
            for value in point:number(value,-1e6,1e6)
        if color_range is not None:
            if not isinstance(color_range,list) or len(color_range)!=2: raise SessionError('color_range requires min,max')
            for value in color_range:number(value,-1e12,1e12)
            if color_range[0]>=color_range[1]: raise SessionError('color_range must increase')
        payload=dict(request_id=request_id,plane=plane,position=position,resolution=resolution,
                     field=field,points=points,color_range=color_range,vectors=vectors)
        with self.lock():
            path=self.run_dir(run_id)
            for name in ('sample_requests','sample_results','sample_ids'):(path/name).mkdir(exist_ok=True)
            identity=path/'sample_ids'/f'{request_id}.json'
            response=path/'sample_results'/f'{request_id}.json'
            pending=path/'sample_requests'/f'{request_id}.json'
            if identity.exists():
                if read(identity)!=payload:raise SessionError('sample_request_id_conflict')
            else:
                if self._status(path)['state'] in TERMINAL:raise SessionError('live_sampling_unavailable_after_terminal; use retained samples and result artifacts')
                if self._status(path).get('sample_protocol') != 1:raise SessionError('worker_has_no_live_sampling; start a new S2 run')
                # Expire abandoned pending requests and cap retained diagnostic requests.
                for old in (path/'sample_requests').glob('*.json'):
                    if time.time()-old.stat().st_mtime>30:old.unlink()
                if len(list((path/'sample_requests').glob('*.json')))>=8:raise SessionError('sample_queue_full; retry later')
                entries=sorted((path/'sample_ids').glob('*.json'),key=lambda p:p.stat().st_mtime)
                retained=len(entries)
                for old in entries:
                    if retained<32:break
                    if (path/'sample_requests'/old.name).exists():continue
                    (path/'sample_results'/old.name).unlink(missing_ok=True);old.unlink();retained-=1
                if retained>=32:raise SessionError('sample_retention_busy; retry later')
                atomic(identity,payload);atomic(pending,payload)
        deadline=time.monotonic()+wait_ms/1000
        while not response.exists() and time.monotonic()<deadline:time.sleep(.01)
        if not response.exists():
            state=self.run_inspect(run_id)['state']
            return dict(request_id=request_id,run_id=run_id,status='unavailable' if state in TERMINAL or not pending.exists() else 'pending',state=state)
        result=read(response)
        preview=result['preview'];preview.update(field=field,color_range=color_range,vectors=vectors)
        result['sample_age_seconds']=max(0,time.time()-result['sampled_at'])
        return result
