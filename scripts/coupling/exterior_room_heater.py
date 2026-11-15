"""Exterior continuation authoring and display; no numerical solver replacement."""
import copy,math
from . import exterior_room_geometry as geometry,rig_geometry,room_heater
from surface_sources.growth_fire_v1 import require


def configuration(base,profile,*,far=True,fine=True,buoyant=True):
    c=copy.deepcopy(base);length=2. if far else 1.5;n=(64 if far else 48) if fine else (32 if far else 24)
    c['grid']=[n]*3;c['length_m']=[length]*3
    c['rig_geometry']=dict(schema=geometry.SCHEMA,window_profile=profile,exterior_profile='far' if far else 'near')
    c['open_domain_forcing']['knots']=[dict(step=0,target_m_s=0,strength=0)];c['buoyancy']['enabled']=buoyant
    c['initial_energy_j']=[0.]*(n**3);c['initial_smoke_kg']=[0.]*(n**3);c['initial_face_velocity_m_s']=[0.]*(3*(n+1)*n*n)
    c['resource_limits']['max_cells']=max(32768,n**3)
    rig_geometry.components(c,geometry.topology(c));return c


def source(config,power_w=20.,smoke_rate=1e-6):
    n,length,o,_=geometry.parameters(config);h=length/n;mask=geometry.topology(config)
    support=[q for q in range(n**3) if .25<=(q%n+.5)*h<.5 and o+.375<=(q//n%n+.5)*h<o+.625 and (q//(n*n)+.5)*h<.125]
    require(support and all(not mask[q] for q in support),'exterior heater excludes wall')
    step={k:[0.]*(n**3) for k in ['energy_j','smoke_kg']}
    for q in support:step['energy_j'][q]=power_w*.005/len(support);step['smoke_kg'][q]=smoke_rate*.005/len(support)
    return step


def export_frame(config,cp,directory):
    from pathlib import Path
    from fire_render_diagnostic import plane,material
    from .playback_export import box_surfaces,make_render_request
    from .rig_scene_adapter import receiving_model
    import combined_scene
    directory=Path(directory);directory.mkdir();result=cp['fluid'];n,length,o,_=geometry.parameters(config)
    with receiving_model(combined_scene,result):combined_scene.write_original_vf3d(result,directory,.0002)
    write=room_heater.write;sha=room_heater.sha
    write(directory/'manifest.json',dict(manifest_version=2,frame_contract='vf3d',frames=[dict(frame_index=0,path='frame_000000.vf3d',frame_contract='vf3d',time_seconds=result['fields']['time_s'])]))
    objects=[plane('ground',[length/2,length/2,-.001],length+.25,length+.25),plane('heater',[.375,o+.5,.001],.25,.25)]
    mats=[material('ground',0x081420),material('heater',0xDE9A45)]
    for name,lo,hi in geometry.solid_boxes(config):
        if name in ('y_min','ceiling'):continue
        for obj in box_surfaces({'fluid':{'solid_box':dict(lower_m=lo,upper_m=hi)}},plane):
            obj['object_id']='exterior_room_'+name+'_'+obj['object_id'];objects.append(obj);mats.append(material(obj['object_id'],0x184653))
    scene=dict(schema_family='codework_scene',schema_variant='scene_runtime_v1',schema_version=1,scene_id='room_plus_exterior_no_wind',space_mode_default='3d',unit_system='meters',world_scale=1.,objects=objects,lights=[dict(light_id='interior_key',kind='point',radius=0,position=dict(x=.4,y=o+.8,z=1.25),intensity=4,falloff_distance=8,color=dict(r=.82,g=.94,b=1),enabled=True,moving=False),dict(light_id='exterior_rim',kind='point',radius=0,position=dict(x=1.6,y=o+.9,z=1.7),intensity=3,falloff_distance=8,color=dict(r=.7,g=.85,b=1),enabled=True,moving=False)],extensions={'ray_tracing':{'authoring':{'object_materials':mats}},'room_diagnostic':dict(physics_checkpoint_digest=cp['digest'],display_cutaways=['y_min','ceiling'],source='20W prescribed heater and passive tracer; no wind',exterior_is_simulated=True)})
    request=make_render_request([.375,o+.5,0]);i=request['inspection'];i.update(camera_position=dict(x=2.8,y=o-2.2,z=1.65),camera_look_at=dict(x=.95,y=o+.5,z=.65),camera_zoom=1.05,ambient_strength=.12,top_fill_strength=.15,background_brightness=.08,background_color=dict(r=.012,g=.035,b=.065))
    i['volume_smoke_optics'].update(mass_extinction_m2_kg=60000,scattering_albedo=.98,anisotropy=0.)
    write(directory/'playback_scene_runtime.json',scene);write(directory/'playback_render_request.json',request)
    return dict(revision=cp['revision'],time_s=result['fields']['time_s'],checkpoint_digest=cp['digest'],request=directory.name+'/playback_render_request.json',scene_sha256=sha(directory/'playback_scene_runtime.json'),request_sha256=sha(directory/'playback_render_request.json'),vf3d_sha256=sha(directory/'frame_000000.vf3d'))
