"""Fixed rig adapter, reusing independent open-model SI/capture checks unchanged."""
from pathlib import Path
import copy
import hashlib
import json
import math
from atmosphere_attempt import retained_atmosphere,retained_directory
from build_owner import owned_worker
from passive_atmosphere import ROOT,run_atmosphere_worker
import open_atmosphere as legacy
from numeric_digest_stream import digest,sealed,strict_load
from surface_sources.growth_fire_v1 import require
from . import rig_geometry


def baseline(config,state=None,steps=()):
    c={k:v for k,v in config.items() if k!='rig_geometry'}
    wrapped=copy.deepcopy(state)
    if wrapped is not None:
        wrapped=sealed({k:v for k,v in wrapped.items() if k!='digest'})
        wrapped['config_digest']=digest(c);wrapped=sealed(wrapped)
    return dict(c,schema=legacy.SCHEMA,state=wrapped,steps=list(steps))


def policy(config,state=None,steps=()):
    from . import exterior_work30
    if exterior_work30.selected(config):
        exterior_work30.validate(config,state,steps);return exterior_work30.policy()
    return legacy

def validate(config,state=None):
    mask=rig_geometry.topology(config)
    require((config['rig_geometry']['schema']=='physics_sim_exterior_room/v1' or config['grid'] in ([16]*3,[32]*3)) and config['momentum_dt_s']==.005,'rig numerical profile')
    if state is not None:
        require(state['digest']==digest(state) and state['config_digest']==digest(config),'rig native state/config identity')
        rig_geometry.check(config,state['data'],mask)
    policy(config,state).validate(baseline(config,state))
    rig_geometry.check(config,dict(energy_j=config['initial_energy_j'],smoke_kg=config['initial_smoke_kg'],pressure_pa=[0.]*len(mask),face_velocity_m_s=config['initial_face_velocity_m_s']),mask)
    return mask


def conservation(config,result):
    from .fluid_adapter import conservation as common
    common(config,result)
    rig_geometry.check(config,result['state']['data'])


@owned_worker(ROOT)
@retained_atmosphere(ROOT,'rig')
def native(request,worker,*,io_format='field_packet/v1'):
    config=request['configuration'];state=request['state'];steps=request['steps'];mask=validate(config,state)
    for step in steps:
        for key in ('energy_j','smoke_kg'):
            require(len(step[key])==len(mask) and all(v==0 for v,s in zip(step[key],mask) if s),'rig source/solid exclusion')
    worker=Path(worker);identity=hashlib.sha256(worker.read_bytes()).hexdigest()
    if state is not None:require(state['worker_sha256']==identity,'rig worker changed')
    request_native=dict(config,schema=legacy.SCHEMA,state=None if state is None else state['data'],steps=steps)
    from field_packet import encode,decode
    limits=config['resource_limits'];limit=limits['native_output_bytes']
    with retained_directory() as directory:
        p=Path(directory)/'request.fpk';p.write_bytes(encode(request_native,limits['native_request_bytes']))
        from . import exterior_work30
        if exterior_work30.selected(config):
            exterior_work30.validate(config,state,steps)
            from .exterior_activity_capture import run_worker
            from passive_atmosphere import worker_subprocess_descriptors
            raw=run_worker(worker,p,limit,worker_subprocess_descriptors())
        else:raw=run_atmosphere_worker(worker,p,limit,io_format='field_packet/v1')
        (Path(directory)/'fields.fpk').write_bytes(raw);fields=decode(raw,limit)
    require(fields.get('solid_mask')==mask,'native/independent rig geometry identity')
    rig_geometry.check(config,fields['state'],mask)
    result=policy(config,state,steps).accept_fields(baseline(config,state,steps),fields,identity)
    result['configuration']=config
    result['state']=sealed(dict(result['state'],config_digest=digest(config)))
    result.update(model=rig_geometry.MODEL,boundaries='closed_insulated_no_slip_shell_with_declared_x_ports',
                  forcing_region=('broad_sweeping_shear/v1: upstream chamber x<=0.4375m; interior y/z; spatial multiplier0.3..1 with3s/4s moving shear; X body-force only' if config['rig_geometry'].get('flow_profile')=='broad_sweeping_shear/v1' else 'west port and upstream strip x<=0.125m; y/z in[0.25,0.75)'),rig_probes=rig_geometry.probes(config,result['state']['data']))
    if config['rig_geometry']['schema']=='physics_sim_exterior_room/v1':
        result.update(boundaries='single_window_internal_to_room_plus_exterior; distant_clean_ambient; solid_ground',forcing_region='none; explicit zero-strength weather driver')
    result=sealed(result);validate(config,result['state']);conservation(config,result)
    require(hashlib.sha256(worker.read_bytes()).hexdigest()==identity,'rig worker bytes changed')
    return result


def run(config,state,steps,worker):
    # The retained wrapper owns exact request and terminal native-capture evidence.
    return native(dict(configuration=config,state=state,steps=steps),worker,io_format='field_packet/v1')
