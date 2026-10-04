"""MCP tool catalog and shared argument validation (no solver policy)."""
import inspect
from service import SessionError

ID = {'type': 'string', 'pattern': '^[A-Za-z0-9_-]{1,64}$'}
REV = {'type': 'string', 'pattern': '^[a-f0-9]{64}$'}
GRID = {'type': 'array', 'items': {'type': 'integer', 'minimum': 1, 'maximum': 256}, 'minItems': 3, 'maxItems': 3}
WAIT = {'type': 'integer', 'minimum': 0, 'maximum': 5000, 'default': 0}
PHYSICS = {
    'fluid': {'type':'object','required':['density_kg_m3','dynamic_viscosity_pa_s'],
              'additionalProperties':False,'properties':{
                  'density_kg_m3':{'type':'number','minimum':.001,'maximum':50000},
                  'dynamic_viscosity_pa_s':{'type':'number','minimum':1e-9,'maximum':1000}}},
    'solver_iterations': {'type':'integer','minimum':8,'maximum':512,'default':20,'description':'Maximum pressure iterations; values above 48 require qualification_mode.'},
    'numerical_memory_limit_mib': {'type':'integer','minimum':1,'maximum':4096,'default':512,'description':'Refined solver numerical allocation limit, excluding total process RSS and JSON.'},
    'qualification_mode': {'type':'boolean','default':False}}

CATALOG = [
    ('capabilities', 'Discover supported local models, scene templates, controls and limits.', {}, []),
    ('scene_create', 'Create an immutable scene from Wind geometry templates or the CFD channel/experimental obstacle presets. Repeat the same ID and parameters safely.',
     {'scene_id': ID, 'template': {'type': 'string', 'enum': ['wind_box','wind_sphere','wind_stl_cube','wind_stl_sphere','wind_stl_cone','wind_empty','cfd_channel','cfd_channel_2d','cfd_obstacle_2d','cfd_open_channel_2d','cfd_open_obstacle_2d','cfd_refined_channel_2d','cfd_refined_obstacle_2d','cfd_duct_3d','cfd_manufactured_3d','cfd_open_duct_3d','cfd_wall_stokes_3d','cfd_wall_transport_3d','cfd_pressure_startup_3d','cfd_open_wall_transient_3d','cfd_obstacle_3d','cfd_box_3d'], 'default':'wind_box'},
      'dimensions': {'type':'array','items':{'type':'number','minimum':.1,'maximum':100},'minItems':3,'maxItems':3},
      'channel': {'type':'object','additionalProperties':False,'properties':{
          'solve_mode':{'type':'string','enum':['steady_stokes','transient_navier_stokes','steady_duct','manufactured_transient','steady_open_duct','wall_stokes_transient','wall_transport_transient','pressure_startup','open_wall_stokes_transient','steady_obstacle_duct','steady_box_duct']},
          'refinement_regions':{'type':'array','maxItems':64,'items':{'type':'object','additionalProperties':False,'required':['bounds_m','level'],'properties':{'bounds_m':{'type':'array','items':{'type':'number'},'minItems':4,'maxItems':4},'level':{'type':'integer','minimum':0,'maximum':10}}}},
          'inlet_mean_m_s':{'type':'number','minimum':.00001,'maximum':100},
          'obstacle_bounds_m':{'type':'array','items':{'type':'number'},'minItems':4,'maxItems':4},
          'initial_divergence_perturbation_m_s':{'type':'number','minimum':-1,'maximum':1},
          'center_x_m':{'type':'number','minimum':.5,'maximum':99.5},
          'body_min_m':{'type':'array','items':{'type':'number'},'minItems':3,'maxItems':3,'description':'Explicit lower aligned box planes in metres; cfd_box_3d only.'},
          'body_max_m':{'type':'array','items':{'type':'number'},'minItems':3,'maxItems':3,'description':'Explicit upper aligned box planes in metres; cfd_box_3d only.'},
          'volume_flow_m3_s':{'type':'number','minimum':1e-9,'maximum':1e4},
          'pressure_gradient_pa_m':{'type':'number','minimum':-10000,'maximum':10000},
          'wall_bottom_m_s':{'type':'number','minimum':-100,'maximum':100},
          'wall_top_m_s':{'type':'number','minimum':-100,'maximum':100}}},
      'object_center_m': {'type':'array','items':{'type':'number'},'minItems':3,'maxItems':3},
      'object_size_m': {'type':'number','minimum':.001},
      'inflow_speed': {'type':'number','minimum':.00001,'maximum':100,'default':2}}, ['scene_id']),
    ('scene_validate', 'Resolve actual grid and estimated storage using the real solver initializer without advancing time.',
     {'scene_id':ID,'scene_revision':REV,'grid':GRID,'dt':{'type':'number','minimum':.00001,'maximum':.1},'solver_cell_budget':{'type':'integer','minimum':0,'maximum':262144},**PHYSICS}, ['scene_id','scene_revision']),
    ('run_start', 'Start one background local run bound to an immutable scene revision. Request ID is the run ID and is idempotent. Starts paused by default.',
     {'request_id':ID,'scene_id':ID,'scene_revision':REV,'grid':GRID,
      'steps':{'type':'integer','minimum':1,'maximum':100000,'default':100},
      'dt':{'type':'number','minimum':.00001,'maximum':.1,'default':.0166666667},
      'start_paused':{'type':'boolean','default':True},
      'solver_cell_budget':{'type':'integer','minimum':0,'maximum':16777216,'default':0},**PHYSICS},
     ['request_id','scene_id','scene_revision']),
    ('run_control', 'Submit ordered pause, step, continue or cancel. A pending receipt is not acknowledgement. Repeat the command ID to retrieve its result. Step requires paused and advances one fixed tick.',
     {'run_id':ID,'command_id':ID,'action':{'type':'string','enum':['pause','step','continue','cancel']},
      'scene_revision':REV,'wait_ms':WAIT}, ['run_id','command_id','action','scene_revision']),
    ('run_inspect', 'Read a coherent solver snapshot; optional bounded sparse XY field preview. Does not export a volume.',
     {'run_id':ID,'preview':{'type':'boolean','default':False},
      'history':{'type':'boolean','default':False},
      'log_tail_lines':{'type':'integer','minimum':0,'maximum':100,'default':0}}, ['run_id']),
    ('run_events', 'Read at most 100 state/command events after a cursor, optionally waiting up to five seconds. Includes current compact status.',
     {'run_id':ID,'after_cursor':{'type':'integer','minimum':0,'default':0},'wait_ms':WAIT}, ['run_id']),
    ('run_sample', 'Request a coherent live sparse field slice and up to 16 XYZ meter probes at a safe boundary. Retry the same request ID to collect pending results. Independent of simulation controls. Retains 32 requests; terminal runs permit cached requests only. Wind pressure is a proxy. Pressure semantics are model-specific: prescribed reduced-channel pressure, MAC total channel pressure, or obstacle periodic correction. Open CFD pressure is solved in Pa with zero outlet datum. Force qualification is case-specific; arbitrary runs remain unqualified.',
     {'run_id':ID,'request_id':ID,'plane':{'type':'string','enum':['XY','XZ','YZ'],'default':'XY'},
      'position':{'type':'number','minimum':0,'maximum':1,'default':.5},
      'resolution':{'type':'integer','minimum':4,'maximum':64,'default':48},
      'field':{'type':'string','enum':['speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity','pressure_pa','shear_stress_pa']},
      'points':{'type':'array','maxItems':16,'items':{'type':'array','minItems':3,'maxItems':3,'items':{'type':'number'}}},
      'color_range':{'type':'array','minItems':2,'maxItems':2,'items':{'type':'number'}},
      'vectors':{'type':'boolean','default':False},'wait_ms':WAIT},['run_id','request_id']),
    ('run_assess', 'Assess open/refined 2D or bounded Cartesian 3D CFD numerical health, scoped physical checks and optional matched refinement. Missing evidence remains not established; passing is not a physical accuracy certificate.',
     {'run_id':ID,'spatial_coarse_run':ID,'temporal_coarse_run':ID}, ['run_id']),
    ('run_compare', 'Compare two or three completed CFD runs with matched scene, fluid, worker and physical time. Reports refinement sensitivity, not physical accuracy.',
     {'run_ids':{'type':'array','minItems':2,'maxItems':3,'items':ID},'kind':{'type':'string','enum':['spatial','temporal'],'default':'spatial'}}, ['run_ids']),
    ('run_result', 'Retrieve a terminal outcome and digest-bound artifact manifest, including exact scene and executable provenance.',
     {'run_id':ID}, ['run_id']),
]


def tools():
    return [{'name':name,'description':description,
             'inputSchema':{'type':'object','properties':props,'required':required,'additionalProperties':False},
             'annotations':{'readOnlyHint':name in ('capabilities','run_inspect','run_events','run_assess'),
                            'idempotentHint':True,'openWorldHint':False}}
            for name,description,props,required in CATALOG]


def call(service, name, arguments):
    spec = next((s for s in CATALOG if s[0] == name), None)
    if spec is None:
        raise SessionError('unknown tool')
    if not isinstance(arguments, dict):
        raise SessionError('arguments must be an object')
    if set(arguments) - set(spec[2]) or set(spec[3]) - set(arguments):
        raise SessionError('unknown or missing arguments')
    for key,value in arguments.items():
        kind = spec[2][key]['type']
        if kind == 'boolean' and not isinstance(value,bool):
            raise SessionError(key+' must be boolean')
        if kind == 'string' and not isinstance(value,str):
            raise SessionError(key+' must be string')
    return getattr(service,name)(**arguments)
