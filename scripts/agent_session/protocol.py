"""MCP tool catalog and shared argument validation (no solver policy)."""
import inspect
from service import SessionError

ID = {'type': 'string', 'pattern': '^[A-Za-z0-9_-]{1,64}$'}
REV = {'type': 'string', 'pattern': '^[a-f0-9]{64}$'}
GRID = {'type': 'array', 'items': {'type': 'integer', 'minimum': 4, 'maximum': 256}, 'minItems': 3, 'maxItems': 3}
WAIT = {'type': 'integer', 'minimum': 0, 'maximum': 5000, 'default': 0}
CATALOG = [
    ('capabilities', 'Discover supported local models, scene templates, controls and limits.', {}, []),
    ('scene_create', 'Create an immutable Wind scene project from a box or sphere template. Repeat the same ID and parameters safely.',
     {'scene_id': ID, 'template': {'type': 'string', 'enum': ['wind_box','wind_sphere'], 'default':'wind_box'},
      'dimensions': {'type':'array','items':{'type':'number','minimum':.1,'maximum':100},'minItems':3,'maxItems':3},
      'inflow_speed': {'type':'number','minimum':.01,'maximum':100,'default':2}}, ['scene_id']),
    ('scene_validate', 'Resolve actual grid and estimated storage using the real solver initializer without advancing time.',
     {'scene_id':ID,'scene_revision':REV,'grid':GRID}, ['scene_id','scene_revision']),
    ('run_start', 'Start one background local run bound to an immutable scene revision. Request ID is the run ID and is idempotent. Starts paused by default.',
     {'request_id':ID,'scene_id':ID,'scene_revision':REV,'grid':GRID,
      'steps':{'type':'integer','minimum':1,'maximum':100000,'default':100},
      'dt':{'type':'number','minimum':.00001,'maximum':.1,'default':.0166666667},
      'start_paused':{'type':'boolean','default':True},
      'solver_cell_budget':{'type':'integer','minimum':0,'maximum':16777216,'default':0}},
     ['request_id','scene_id','scene_revision']),
    ('run_control', 'Submit ordered pause, step, continue or cancel. A pending receipt is not acknowledgement. Repeat the command ID to retrieve its result. Step requires paused and advances one fixed tick.',
     {'run_id':ID,'command_id':ID,'action':{'type':'string','enum':['pause','step','continue','cancel']},
      'scene_revision':REV,'wait_ms':WAIT}, ['run_id','command_id','action','scene_revision']),
    ('run_inspect', 'Read a coherent solver snapshot; optional bounded sparse XY field preview. Does not export a volume.',
     {'run_id':ID,'preview':{'type':'boolean','default':False},
      'log_tail_lines':{'type':'integer','minimum':0,'maximum':100,'default':0}}, ['run_id']),
    ('run_events', 'Read at most 100 state/command events after a cursor, optionally waiting up to five seconds. Includes current compact status.',
     {'run_id':ID,'after_cursor':{'type':'integer','minimum':0,'default':0},'wait_ms':WAIT}, ['run_id']),
    ('run_result', 'Retrieve a terminal outcome and digest-bound artifact manifest, including exact scene and executable provenance.',
     {'run_id':ID}, ['run_id']),
]


def tools():
    return [{'name':name,'description':description,
             'inputSchema':{'type':'object','properties':props,'required':required,'additionalProperties':False},
             'annotations':{'readOnlyHint':name in ('capabilities','run_inspect','run_events'),
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
