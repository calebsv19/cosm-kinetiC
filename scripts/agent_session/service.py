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
import shutil
from qualification import FLUIDS, write_shape
from acceptance import assess
from cartesian3d import TEMPLATES as CARTESIAN_TEMPLATES, MODEL as CARTESIAN_MODEL, scene_parameters as cartesian_parameters, assess_3d, comparison_metrics as cartesian_metrics
from refined import TEMPLATES as REFINED_TEMPLATES, MODEL as REFINED_MODEL, scene_parameters as refined_parameters, assess_refined, comparison_metrics
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
        return {'schema': 'physics_sim_session_capabilities_v1', 'models': ['wind_approximate_v1', 'wind_numerical_qualification_v1', 'incompressible_channel_fv_v1','incompressible_mac2d_v1','incompressible_open2d_v1', REFINED_MODEL, CARTESIAN_MODEL],
                'templates': ['wind_box', 'wind_sphere', 'wind_stl_cube', 'wind_stl_sphere', 'wind_stl_cone', 'wind_empty', 'cfd_channel','cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d',*REFINED_TEMPLATES,*CARTESIAN_TEMPLATES], 'fluid_presets': FLUIDS, 'qualification': {'si_viscosity': True, 'drag_force_validated': False, 'wind_heuristics_can_disable': True}, 'active_runs_per_root': 1,
                'controls': ['pause', 'step', 'continue', 'cancel'], 'checkpoint_restart': False,
                'inspection': {'planes':['XY','XZ','YZ'],'fields':['speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity','pressure_pa','shear_stress_pa'], 'max_samples':4096,'max_probes':16,'history_points':128,'pending_requests':8,'retained_requests':32},
                'states': ['starting','running','paused','completed','cancelled','failed'],
                'tick_semantics': 'one fixed dt, including configured core_sim substeps; acknowledgement at safe boundaries',
                'preview': 'XY midpoint, at most 64x64 sparse samples; [speed,dye_density,solid]',
                'cfd_lab': {'presets':['cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d'],'obstacle_status':'periodic: fixed-case force consistency; open: confined low-Re total-drag reference screen; arbitrary runs unqualified','open_boundary_status':'agent integrated; run_assess checks numerical readiness, not physical certification'},
                'mac2d_model': {'template':'cfd_channel_2d','grid':'[Nx,Ny,1], 4..64 per axis','pressure':'solved periodic correction in Pa plus prescribed affine mean','scope':'2D laminar MAC, periodic X and no-slip Y','substep_budget':1024},
                'cartesian3d_model':{'templates':list(CARTESIAN_TEMPLATES),'grid':'[Nx,Ny,Nz], 4..256; <=262144 cells','solve_modes':['steady_duct','manufactured_transient','steady_open_duct','wall_stokes_transient','wall_transport_transient','pressure_startup','open_wall_stokes_transient','steady_obstacle_duct','steady_box_duct'],'boundary_scope':'periodic/steady baselines plus wall Stokes/transport, pressure startup and controlled natural-traction open transient; wake/backflow unqualified','numerical_memory_limit_mib':[1,4096],
                    'geometry_scope':'parameterized verification templates; cfd_obstacle_3d is an aligned 1 m cube; cfd_box_3d accepts explicit aligned stationary body_min_m/body_max_m in a 2x2 m duct, at least two body/fluid cells per direction, bulk body Re <=0.1; arbitrary scene objects are unsupported',
                    'assessment_scope':'per-run numerical/reference checks; obstacle accuracy remains not established and multi-run certification is separate',
                    'steady_tick_semantics':'one stationary solve; physical time remains zero',
                    'inspection_state':'last accepted fields during private candidate solve'},
                'refined2d_model': {'templates':list(REFINED_TEMPLATES),'base_grid':'[Nx,Ny,1], 4..256','solve_modes':['transient_navier_stokes','steady_stokes'],'max_regions':64,'max_level':10,'max_fluid_leaves':200000,'numerical_memory_limit_mib':[1,4096],'physical_scope':'fixed reference gates only; no arbitrary transient accuracy certificate'},
                'channel_model': {'template':'cfd_channel','grid':'[1,N,1], 4<=N<=256','characteristic_reynolds_max':100,'pressure':'prescribed affine pressure in Pa','scope':'fully developed parallel-wall laminar flow; no general pressure solve'},
                'model_limitations': 'Model-specific: Wind remains unvalidated predictive CFD; channel is a reduced laminar verification model',
                'root': str(self.root), 'worker_available': self.worker.is_file()}

    def scene_create(self, scene_id, template='wind_box', dimensions=None, inflow_speed=2.0, object_size_m=None, object_center_m=None, channel=None):
        scene_id = identifier(scene_id)
        if template not in ('wind_box', 'wind_sphere', 'wind_stl_cube', 'wind_stl_sphere', 'wind_stl_cone', 'wind_empty', 'cfd_channel','cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d',*REFINED_TEMPLATES,*CARTESIAN_TEMPLATES):
            raise SessionError('unsupported scene template')
        defaults = ([4.0, 2.0, 2.0] if template in ('cfd_pressure_startup_3d', 'cfd_open_wall_transient_3d','cfd_obstacle_3d','cfd_box_3d')
                    else [2.0, 2.5, 3.0] if template in ('cfd_wall_stokes_3d', 'cfd_wall_transport_3d')
                    else [4.0, 2.0, .5] if template in ('cfd_obstacle_2d', 'cfd_open_obstacle_2d', 'cfd_refined_obstacle_2d')
                    else [2.0, 1.0, 1.0])
        dimensions = dimensions if dimensions is not None else defaults
        if not isinstance(dimensions, list) or len(dimensions) != 3:
            raise SessionError('dimensions must contain three lengths in meters')
        lengths = [number(x, 0.1, 100) for x in dimensions]
        speed = number(inflow_speed, 0.00001, 100)
        size = min(lengths)*.18 if object_size_m is None else number(object_size_m, .001, min(lengths)*.5)
        center = [lengths[0]*.45, lengths[1]*.5, lengths[2]*.5] if object_center_m is None else object_center_m
        if not isinstance(center,list) or len(center)!=3:
            raise SessionError('object_center_m must contain three coordinates')
        center=[number(v,size/2,lengths[a]-size/2) for a,v in enumerate(center)]
        # This is a bounded authoring template, not an arbitrary runtime-JSON editor.
        doc = {'schema_family': 'codework_scene', 'schema_variant': 'scene_runtime_v1',
               'schema_version': 1, 'scene_id': scene_id, 'space_mode_default': '3d',
               'unit_system': 'meters', 'world_scale': 1.0,
               'objects': [{'object_id': 'obstacle', 'object_type': 'box' if template == 'wind_box' else 'circle',
                            'transform': {'position': dict(zip('xyz', center)),
                                          'scale': dict(zip('xyz', [size]*3)),
                                          'rotation': dict.fromkeys('xyz', 0.0)}, 'flags': {'locked': True}}],
               'materials': [], 'lights': [], 'cameras': [], 'constraints': [],
               'extensions': {'physics_sim': {
                   'scene_domain': {'active': True, 'shape': 'box', 'min': dict.fromkeys('xyz', 0.0),
                                    'max': dict(zip('xyz', lengths))},
                   'wind_tunnel': {'active': True, 'inlet_face': 'left', 'outlet_face': 'right',
                                   'inflow_speed': speed, 'inflow_density': .75, 'inlet_slab_cells': 2,
                                   'outlet_policy': 'receive', 'wall_policy': 'no_slip'}}}}
        if template == 'wind_empty': doc['objects'] = []
        if template.startswith('wind_stl_'):
            obj=doc['objects'][0]
            obj['object_type']='mesh_asset_instance'
            obj['geometry_ref']={'kind':'mesh_asset','id':'qualification_shape'}
            obj['extensions']={'line_drawing':{'runtime_mesh_path':'assets/shape.runtime.json'},
                               'physics_sim':{'fluid_behavior':'solid_obstacle','fluid_obstacle':True,
                                              'mesh_proxy_mode':'surface_and_closed_fill'}}
            # Binds generator version and selected shape before compilation.
            doc['extensions']['physics_sim']['qualification_geometry']={'shape':template[9:],'generator':1,'size_m':size}
        if template in CARTESIAN_TEMPLATES:
            if inflow_speed!=2.0 or object_size_m is not None or object_center_m is not None:
                raise SessionError('bounded 3D uses verification channel parameters')
            doc['objects']=[];doc['extensions']['physics_sim'].pop('wind_tunnel')
            doc['extensions']['physics_sim']['channel_flow']=cartesian_parameters(template,lengths,channel,number,SessionError)
        elif template in REFINED_TEMPLATES:
            if inflow_speed!=2.0 or object_size_m is not None or object_center_m is not None:
                raise SessionError('refined CFD uses channel inlet and obstacle bounds')
            doc['objects']=[];doc['extensions']['physics_sim'].pop('wind_tunnel')
            doc['extensions']['physics_sim']['channel_flow']=refined_parameters(template,lengths,channel,number,SessionError)
        elif template in ('cfd_open_channel_2d','cfd_open_obstacle_2d'):
            if inflow_speed!=2.0:raise SessionError('open CFD inlet uses channel.inlet_mean_m_s')
            params={'inlet_mean_m_s':.002 if template=='cfd_open_obstacle_2d' else .05}
            if template=='cfd_open_obstacle_2d':params['obstacle_bounds_m']=[.375*lengths[0],.375*lengths[1],.625*lengths[0],.625*lengths[1]]
            if channel is not None:
                if not isinstance(channel,dict) or set(channel)-set(params):raise SessionError('unsupported open CFD parameters')
                params.update(channel)
            params['inlet_mean_m_s']=number(params['inlet_mean_m_s'],.00001,100)
            if 'obstacle_bounds_m' in params:
                bounds=params['obstacle_bounds_m']
                if not isinstance(bounds,list) or len(bounds)!=4:raise SessionError('obstacle_bounds_m requires four bounds')
                bounds=[number(x,0,lengths[k%2]) for k,x in enumerate(bounds)]
                if not(bounds[0]<bounds[2] and bounds[1]<bounds[3]):raise SessionError('obstacle requires positive area')
                params['obstacle_bounds_m']=bounds
            params['solver_model']='incompressible_open2d_v1'
            doc['objects']=[];doc['extensions']['physics_sim'].pop('wind_tunnel')
            doc['extensions']['physics_sim']['channel_flow']=dict(params,dimensions_m=lengths)
        elif template in ('cfd_channel','cfd_channel_2d','cfd_obstacle_2d'):
            if object_size_m is not None or object_center_m is not None: raise SessionError('channel has no obstacles')
            if inflow_speed!=2.0: raise SessionError('channel uses a pressure gradient, not prescribed inflow_speed')
            params={'pressure_gradient_pa_m':1.0,'wall_bottom_m_s':0.0,'wall_top_m_s':0.0}
            if template in ('cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d'):params['initial_divergence_perturbation_m_s']=0.0
            if template=='cfd_obstacle_2d':
                params['pressure_gradient_pa_m']=.01
                params['obstacle_bounds_m']=[.375*lengths[0],.375*lengths[1],.625*lengths[0],.625*lengths[1]]
            if channel is not None:
                if not isinstance(channel,dict) or set(channel)-set(params): raise SessionError('unsupported channel parameters')
                params.update(channel)
            bounds=params.pop('obstacle_bounds_m',None)
            for k in params: params[k]=number(params[k],-10000 if k=='pressure_gradient_pa_m' else -100,10000 if k=='pressure_gradient_pa_m' else 100)
            if bounds is not None:
                if not isinstance(bounds,list) or len(bounds)!=4:raise SessionError('obstacle_bounds_m requires [xmin,ymin,xmax,ymax]')
                bounds=[number(x,0,lengths[i%2]) for i,x in enumerate(bounds)]
                if not (bounds[0]<bounds[2] and bounds[1]<bounds[3]):raise SessionError('obstacle bounds must have positive area')
                if params['initial_divergence_perturbation_m_s']!=0:raise SessionError('obstacle must initialize at rest')
                params['obstacle_bounds_m']=bounds
            if template in ('cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d'):
                params['initial_divergence_perturbation_m_s']=number(params['initial_divergence_perturbation_m_s'],-1,1)
                params['solver_model']='incompressible_mac2d_v1'
            doc['objects']=[]
            doc['extensions']['physics_sim'].pop('wind_tunnel')
            doc['extensions']['physics_sim']['channel_flow']=dict(params,dimensions_m=lengths)
        elif channel is not None: raise SessionError('channel parameters require cfd_channel template')
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
                    assets = write_shape(stage, template[9:]) if template.startswith('wind_stl_') else {}
                    atomic(stage / 'scene_authoring.json', dict(doc, schema_variant='scene_authoring_v1'))
                    compiled = subprocess.run([str(self.worker),'--compile',str(stage / 'scene_authoring.json'),
                                               str(stage / 'scene_runtime.json')], capture_output=True,text=True,timeout=30)
                    if compiled.returncode:
                        raise SessionError('scene_compile_failed: '+compiled.stderr[-1000:])
                    revision = digest(read(stage / 'scene_runtime.json'))
                    atomic(stage / 'scene.json', {'scene_id':scene_id,'revision':revision,'template':template,
                                                 'template_fingerprint':template_fingerprint, 'assets':assets})
                    os.replace(stage,path)
        return {'scene_id': scene_id, 'scene_revision': revision, 'project_path': str(path)}

    def request(self, scene_id, scene_revision, grid=None, steps=100, dt=.0166666667,
                start_paused=True, solver_cell_budget=0, fluid=None, solver_iterations=20,
                qualification_mode=False, numerical_memory_limit_mib=512):
        scene = self.root / 'scenes' / identifier(scene_id)
        if not (scene / 'scene_runtime.json').exists():
            raise SessionError('unknown scene_id')
        doc = read(scene / 'scene_runtime.json')
        if digest(doc) != scene_revision or read(scene / 'scene.json')['revision'] != scene_revision:
            raise SessionError('stale_scene_revision')
        channel=doc.get('extensions',{}).get('physics_sim',{}).get('channel_flow')
        template=read(scene/'scene.json')['template']
        is_cartesian=template in CARTESIAN_TEMPLATES
        is_refined=template in REFINED_TEMPLATES
        is_open=template in ('cfd_open_channel_2d','cfd_open_obstacle_2d')
        is_mac=is_refined or template in ('cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d')
        is_channel=is_cartesian or is_refined or template in ('cfd_channel','cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d')
        if is_channel and not channel: raise SessionError('compiled channel configuration missing')
        grid = grid if grid is not None else ([16,8,8] if is_cartesian else ([16,16,1] if is_mac else ([1,32,1] if is_channel else [32,16,16])))
        if not isinstance(grid, list) or len(grid) != 3:
            raise SessionError('grid must contain three integers')
        grid = [number(x, 1 if is_channel else 4, 256, True) for x in grid]
        if is_refined and not (4<=grid[0]<=256 and 4<=grid[1]<=256 and grid[2]==1):raise SessionError('refined grid must be [Nx,Ny,1], 4..256 base cells per axis')
        if is_mac and not is_refined and not (4<=grid[0]<=64 and 4<=grid[1]<=64 and grid[2]==1):raise SessionError('2D channel grid must be [Nx,Ny,1] with 4..64 cells per axis')
        if is_channel and not is_mac and not is_cartesian and (grid[0]!=1 or grid[2]!=1 or grid[1]<4): raise SessionError('channel grid must be [1, N>=4, 1]; x/z are invariant')
        if is_mac and not is_refined and 'obstacle_bounds_m' in channel:
            bounds=channel['obstacle_bounds_m'];dims=channel['dimensions_m']
            indices=[bounds[k]*grid[k%2]/dims[k%2] for k in range(4)]
            if any(abs(x-round(x))>1e-8 for x in indices):raise SessionError('obstacle must align with grid cell boundaries; no silent snapping')
            pad=3 if is_open else 2
            if not (pad<=indices[0]<indices[2]<=grid[0]-pad and pad<=indices[1]<indices[3]<=grid[1]-pad):raise SessionError(f'obstacle requires {pad} fluid cells to outer boundaries')
        if not isinstance(start_paused, bool):
            raise SessionError('start_paused must be boolean')
        if not isinstance(qualification_mode, bool): raise SessionError('qualification_mode must be boolean')
        physical = None
        if fluid is not None:
            if not isinstance(fluid, dict) or set(fluid) != {'density_kg_m3','dynamic_viscosity_pa_s'}:
                raise SessionError('fluid requires density_kg_m3 and dynamic_viscosity_pa_s')
            physical = {'density_kg_m3':number(fluid['density_kg_m3'],.001,50000),
                        'dynamic_viscosity_pa_s':number(fluid['dynamic_viscosity_pa_s'],1e-9,1000)}
        if qualification_mode and physical is None: raise SessionError('qualification_mode requires explicit SI fluid')
        if is_channel:
            if physical is None: raise SessionError('channel requires explicit SI fluid')
            if qualification_mode or (solver_cell_budget and not is_refined and not is_cartesian) or solver_iterations!=20: raise SessionError('Wind qualification/CG/budget options do not apply to channel')
            rho=physical['density_kg_m3'];mu=physical['dynamic_viscosity_pa_s'];height=channel['dimensions_m'][1]
            if is_cartesian:
                if min(grid)<4 or math.prod(grid)>262144:raise SessionError('bounded 3D grid requires 4..256 cells per axis and at most 262144 cells')
                solver_cell_budget=number(solver_cell_budget or 262144,1,262144,True)
                if math.prod(grid)>solver_cell_budget:raise SessionError('cartesian3d_cell_budget_exceeded')
                if channel['solve_mode'] in ('steady_duct','steady_open_duct','pressure_startup','steady_obstacle_duct','steady_box_duct'):
                    if channel['solve_mode']!='pressure_startup' and steps!=1:raise SessionError('steady_duct requires one stationary solve step')
                    speed=channel['volume_flow_m3_s']/(height*channel['dimensions_m'][2])
                    if channel['solve_mode']=='steady_obstacle_duct':
                        if rho*speed/mu>.1:raise SessionError('cube duct requires creeping mean Re <=.1')
                        lows=[channel['center_x_m']-.5,.5,.5];highs=[x+1 for x in lows]
                        for axis in range(3):
                            spacing=channel['dimensions_m'][axis]/grid[axis]
                            indices=[lows[axis]/spacing,highs[axis]/spacing]
                            if any(abs(x-round(x))>1e-8 for x in indices) or indices[0]<2 or indices[1]>grid[axis]-2:raise SessionError('cube must align exactly with at least two fluid cells to walls')
                    if channel['solve_mode']=='steady_box_duct':
                        lows=channel['body_min_m'];highs=channel['body_max_m']
                        characteristic=max(hi-lo for lo,hi in zip(lows,highs))
                        if rho*speed*characteristic/mu>.1:raise SessionError('box duct requires creeping bulk body Re <=.1')
                        for axis in range(3):
                            spacing=channel['dimensions_m'][axis]/grid[axis]
                            lo,hi=lows[axis]/spacing,highs[axis]/spacing
                            if (abs(lo-round(lo))>1e-9 or abs(hi-round(hi))>1e-9 or
                                lo<2 or hi>grid[axis]-2 or round(hi)-round(lo)<2):
                                raise SessionError('box requires exact grid planes, two body cells and two fluid cells to every outer plane; no snapping')
                elif channel['solve_mode']=='manufactured_transient':
                    speed=.024
                    if dt*speed*sum(n/l for n,l in zip(grid,channel['dimensions_m']))>.25:raise SessionError('manufactured 3D initial CFL bound exceeded')
                else:
                    lx,ly,lz=channel['dimensions_m']
                    if channel['solve_mode']=='open_wall_stokes_transient':lx=4
                    bound=[1.2*(.013*math.pi/ly+.007*math.pi/lz),
                           1.2*(.011*math.pi/lz+.013*2*math.pi/lx),
                           1.2*(.007*2*math.pi/lx+.011*math.pi/ly)]
                    speed=max(bound)
                    if channel['solve_mode']=='wall_transport_transient' and dt*sum(v*n/l for v,n,l in zip(bound,grid,channel['dimensions_m']))>.25:
                        raise SessionError('wall transport initial CFL upper bound exceeded')
            elif is_refined:
                speed=channel['inlet_mean_m_s']
                if channel['solve_mode']=='steady_stokes' and steps!=1:raise SessionError('steady_stokes requires exactly one solve step; physical time remains zero')
                solver_cell_budget=number(solver_cell_budget or 200000,1,200000,True)
            elif is_open:
                speed=channel['inlet_mean_m_s']
                dx=channel['dimensions_m'][0]/grid[0];dy=height/grid[1]
                if number(dt,.00001,.1)*(1.5*speed/dx+2*mu/rho*(1/dx**2+1/dy**2))>.4:raise SessionError('open CFD timestep exceeds initial CFL/diffusion bound')
            else:
                speed=max(abs(channel['wall_bottom_m_s']),abs(channel['wall_top_m_s']))+abs(channel['pressure_gradient_pa_m'])*height**2/(8*mu)
                speed+=abs(channel.get('initial_divergence_perturbation_m_s',0))
            if rho*height*speed/mu>100:raise SessionError('channel characteristic Reynolds number exceeds 100')
        if not is_refined and not is_cartesian and numerical_memory_limit_mib!=512:raise SessionError('numerical_memory_limit_mib applies only to refined 2D or bounded 3D CFD')
        self._verify_assets(scene)
        req = {'schema': 'physics_sim_session_request_v1', 'scene_id': scene_id,
               'scene_revision': scene_revision, 'grid': grid, 'steps': number(steps, 1, 100000, True),
               'dt': number(dt, .00001, .1), 'start_paused': start_paused,
               'solver_cell_budget': number(solver_cell_budget, 0, 16777216, True),
               'fluid':physical, 'solver_iterations':number(solver_iterations,8,512 if qualification_mode else 48,True),
               'qualification_mode':qualification_mode}
        if is_channel: req.update(model=CARTESIAN_MODEL if is_cartesian else REFINED_MODEL if is_refined else "incompressible_open2d_v1" if is_open else "incompressible_mac2d_v1" if is_mac else "incompressible_channel_fv_v1",channel=channel)
        if is_refined or is_cartesian:req["numerical_memory_limit_mib"]=number(numerical_memory_limit_mib,1,4096,True)
        return req, doc

    @staticmethod
    def _verify_assets(scene):
        assets=read(scene/'scene.json').get('assets',{})
        for name, expected in assets.items():
            p=scene/name
            if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:
                raise SessionError('scene_asset_digest_mismatch')
        return assets

    def _copy_assets(self, scene_id, destination):
        scene=self.root/'scenes'/scene_id
        for name in self._verify_assets(scene):
            p=destination/name;p.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(scene/name,p)

    def scene_validate(self, scene_id, scene_revision, grid=None, **options):
        options.setdefault("steps",1)
        req, doc = self.request(scene_id, scene_revision, grid, **options)
        with tempfile.TemporaryDirectory(prefix='validate-', dir=self.root) as tmp:
            root = Path(tmp)
            atomic(root / 'request.json', dict(req, run_id='validation'))
            atomic(root / 'scene_runtime.json', doc)
            self._copy_assets(scene_id, root)
            result = subprocess.run([str(self.worker), str(root), '--validate'], capture_output=True,
                                    text=True, timeout=30)
            if result.returncode:
                raise SessionError('scene_validation_failed: ' + result.stderr[-1000:])
            status = json.loads(result.stdout)
            return {k: status[k] for k in ('effective_grid', 'voxel_size_m', 'estimated_dense_bytes', 'model_limitations', 'physics', 'geometry')} | {
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
        if snap['state'] not in TERMINAL:
            # The owner may publish terminal state and exit after the first read.
            # Acquire its released lock, then re-read before reconciling death;
            # never replace completion with a stale running snapshot.
            with (path / 'owner.lock').open('a') as owner:
                try:
                    fcntl.flock(owner, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    return snap
                if snap_path.exists():
                    snap = read(snap_path)
                if snap['state'] in TERMINAL:
                    return snap
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
                    legacy=dict(req)
                    compatible=True
                    for key,default in (('fluid',None),('solver_iterations',20),('qualification_mode',False)):
                        if key not in old:
                            compatible &= legacy[key] == default
                            legacy.pop(key)
                    if not compatible or old['request_fingerprint'] != digest(legacy):
                        raise SessionError('request_id_conflict')
                return self._compact(self._status(path))
            for other in (self.root / 'runs').iterdir():
                if (other / 'request.json').exists() and self._status(other)['state'] not in TERMINAL:
                    raise SessionError('active_run_exists')
            if not self.worker.is_file():
                raise SessionError('build physics_sim_session_worker first')
            req.update(run_id=run_id, request_fingerprint=fingerprint,
                       geometry_assets=self._verify_assets(self.root/'scenes'/scene_id),
                       worker_sha256=hashlib.sha256(self.worker.read_bytes()).hexdigest(),
                       created_at=time.time(), worker_path=str(self.worker))
            path.mkdir()
            for name in ('commands', 'receipts', 'command_ids'):
                (path / name).mkdir()
            atomic(path / 'request.json', req)
            atomic(path / 'scene_runtime.json', doc)
            self._copy_assets(scene_id, path)
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
        files.extend(sorted((path / 'assets').rglob('*')))
        files.extend(sorted((path / 'sample_results').glob('*.json')))
        for file in files:
            if file.is_file():
                artifacts.append({'path': str(file), 'bytes': file.stat().st_size,
                                  'sha256': hashlib.sha256(file.read_bytes()).hexdigest()})
        result = {'status': status, 'provenance': read(path / 'request.json'), 'artifacts': artifacts}
        atomic(path / 'result.json', result)
        return result

    def run_assess(self, run_id, spatial_coarse_run=None, temporal_coarse_run=None):
        status=self.run_inspect(run_id)
        if status.get('model')==CARTESIAN_MODEL:
            comparisons={}
            for kind,coarse in [('spatial',spatial_coarse_run),('temporal',temporal_coarse_run)]:
                if coarse is not None:comparisons[kind]=self.run_compare([coarse,run_id],kind)
            return assess_3d(status,comparisons)
        if status.get('model')==REFINED_MODEL:
            comparisons={}
            for kind,coarse in [('spatial',spatial_coarse_run),('temporal',temporal_coarse_run)]:
                if coarse is not None:comparisons[kind]=self.run_compare([coarse,run_id],kind)
            return assess_refined(status, comparisons)
        if status.get('model')!='incompressible_open2d_v1':
            raise SessionError('acceptance currently supports the open 2D CFD model')
        comparisons={}
        for kind,coarse in [('spatial',spatial_coarse_run),('temporal',temporal_coarse_run)]:
            if coarse is not None:comparisons[kind]=self.run_compare([coarse,run_id],kind)
        return assess(status,comparisons)

    def run_compare(self, run_ids, kind='spatial'):
        if kind not in ('spatial','temporal'):raise SessionError('comparison kind must be spatial or temporal')
        if not isinstance(run_ids,list) or not 2<=len(run_ids)<=3:raise SessionError('compare requires two or three distinct completed run IDs')
        run_ids=[identifier(x) for x in run_ids]
        if len(set(run_ids))!=len(run_ids):raise SessionError('compare requires distinct run IDs')
        records=[]
        for run_id in run_ids:
            status=self.run_inspect(run_id)
            if status['state']!='completed':raise SessionError('comparison requires completed runs')
            request=read(self.run_dir(run_id)/'request.json')
            if request.get('model') not in ('incompressible_mac2d_v1','incompressible_open2d_v1',REFINED_MODEL,CARTESIAN_MODEL):raise SessionError('comparison requires a supported CFD model')
            records.append((request,status))
        base,base_status=records[0]
        if base.get('model')==CARTESIAN_MODEL and kind=='temporal' and base_status.get('solve_mode') in ('steady_duct','steady_open_duct','steady_obstacle_duct','steady_box_duct'):raise SessionError('steady duct has no physical timestep refinement')
        if base.get('model')==REFINED_MODEL and kind=='temporal' and base_status.get('solve_mode')=='steady_stokes':
            raise SessionError('steady Stokes has no physical timestep refinement')
        for req,status in records[1:]:
            for key in ('scene_revision','model','fluid','worker_sha256'):
                if req.get(key)!=base.get(key):raise SessionError('comparison mismatch: '+key)
            if abs(status['simulation_time']-base_status['simulation_time'])>1e-8:raise SessionError('comparison requires matched physical time')
        comparisons=[]
        for (coarse,a),(fine,b) in zip(records,records[1:]):
            if kind=='spatial':
                if coarse['dt']!=fine['dt'] or not all(y>=x and y%x==0 for x,y in zip(coarse['grid'],fine['grid'])) or coarse['grid']==fine['grid']:raise SessionError('spatial comparison needs nested increasing grids and equal dt')
            elif coarse['grid']!=fine['grid'] or fine['dt']>=coarse['dt']:raise SessionError('temporal comparison needs equal grids and decreasing dt')
            changes={}
            for group,keys in [('health',('volume_flux_m3_s','kinetic_energy_j')),('boundary_force_budget',('obstacle_discrete_pressure_force_x_n','obstacle_discrete_viscous_force_x_n','obstacle_reconstructed_pressure_force_x_n','surface_pressure_force_x_n','surface_viscous_force_x_n','surface_total_force_x_n','control_volume_force_x_n'))]:
                for key in keys:
                    x=a.get(group,{}).get(key);y=b.get(group,{}).get(key)
                    if x is None or y is None:continue
                    if not isinstance(x,(int,float)) or not isinstance(y,(int,float)) or not math.isfinite(x) or not math.isfinite(y):raise SessionError('nonfinite comparison metric')
                    changes[key]={'coarse':x,'fine':y,'absolute_difference':abs(y-x),'relative_to_fine':abs(y-x)/abs(y) if y else (0 if x==0 else None)}
            if base.get('model') in (REFINED_MODEL,CARTESIAN_MODEL):
                metrics=cartesian_metrics if base.get('model')==CARTESIAN_MODEL else comparison_metrics
                left,right=metrics(a),metrics(b)
                if not left or left.keys()!=right.keys():raise SessionError('refined comparison observations unavailable or mismatched')
                for key,x in left.items():
                    y=right[key]
                    if any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in (x,y)):raise SessionError('nonfinite comparison metric')
                    changes[key]={'coarse':x,'fine':y,'absolute_difference':abs(y-x),'relative_to_fine':abs(y-x)/abs(y) if y else (0 if x==0 else None)}
            comparisons.append({'coarse_run':coarse['run_id'],'fine_run':fine['run_id'],'changes':changes})
        return {'schema':'physics_sim_cfd_refinement_comparison_v1','kind':kind,'scene_revision':base['scene_revision'],'worker_sha256':base['worker_sha256'],'simulation_time':base_status['simulation_time'],'comparisons':comparisons,'qualification':'sensitivity_only; no reference error or steady-state acceptance inferred','run_statuses':[status.get('boundary_force_budget',{}).get('drag_status') for _,status in records]}

    def run_sample(self, run_id, request_id, plane='XY', position=.5, resolution=48,
                   field='speed', points=None, wait_ms=0, color_range=None, vectors=False):
        identifier(request_id)
        if plane not in ('XY','XZ','YZ'): raise SessionError('plane must be XY, XZ or YZ')
        fields=('speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity','pressure_pa','shear_stress_pa')
        if field not in fields: raise SessionError('unsupported field')
        channel=read(self.run_dir(run_id)/'request.json').get('model') in ('incompressible_channel_fv_v1','incompressible_mac2d_v1','incompressible_open2d_v1',REFINED_MODEL,CARTESIAN_MODEL)
        if (field in ('pressure_pa','shear_stress_pa') and not channel) or (field=='pressure_proxy' and channel):
            raise SessionError('field unavailable for this model; physical channel pressure is pressure_pa')
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
