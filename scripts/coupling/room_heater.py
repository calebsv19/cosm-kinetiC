"""Prescribed-heater native room diagnostic. No Fire bundle or joint-coupling claim."""
from pathlib import Path
import json,math,zlib,hashlib,os
from . import rig_adapter,rig_geometry,room_geometry
from numeric_digest_stream import sealed,digest
from surface_sources.growth_fire_v1 import require

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text())
def write(path,value):
    path=Path(path);temp=path.with_suffix('.pending')
    with temp.open('w') as f:json.dump(value,f,sort_keys=True,allow_nan=False);f.write('\n');f.flush();os.fsync(f.fileno())
    os.replace(temp,path)
def configuration(base,profile):
    c=json.loads(json.dumps(base));c['rig_geometry']=dict(schema=room_geometry.SCHEMA,window_profile=profile)
    c['open_domain_forcing']['knots']=[dict(step=0,target_m_s=0,strength=0)]
    mask=rig_geometry.topology(c);rig_geometry.components(c,mask)
    require(all(v==0 for v in c['initial_energy_j']+c['initial_smoke_kg']+c['initial_face_velocity_m_s']),'room starts at ambient rest')
    return c

def source(config,power_w,smoke_rate):
    n=config['grid'][0];mask=room_geometry.topology(config)
    support=[q for q in range(n**3) if .25<=(q%n+.5)/n<.5 and .375<=(q//n%n+.5)/n<.625 and (q//(n*n)+.5)/n<.125]
    require(support and all(not mask[q] for q in support),'heater physical support excludes walls')
    step={k:[0.]*(n**3) for k in ['energy_j','smoke_kg']}
    for q in support:
        step['energy_j'][q]=power_w*.005/len(support);step['smoke_kg'][q]=smoke_rate*.005/len(support)
    return step

def load(root,revision=None):
    root=Path(root);head=read(root/'head.json');index=len(head['records'])-1 if revision is None else revision
    row=head['records'][index];p=root/'states'/row['file'];require(sha(p)==row['sha256'],'room checkpoint bytes')
    cp=json.loads(zlib.decompress(p.read_bytes()));require(cp==sealed(cp) and cp['digest']==row['digest'] and cp['revision']==index and cp['binding_digest']==digest(read(root/'binding.json')),'room checkpoint identity')
    return cp

def save(root,config,result,revision,parent,records):
    root=Path(root);cp=sealed(dict(schema='prescribed_room_heater_checkpoint/v1',revision=revision,parent_digest=parent,configuration_digest=digest(config),binding_digest=digest(read(root/'binding.json')),fluid=result))
    name='state-%04d.json.zlib'%revision;p=root/'states'/name;require(not p.exists(),'retained checkpoint collision')
    raw=zlib.compress(json.dumps(cp,sort_keys=True,separators=(',',':'),allow_nan=False).encode(),6);temp=p.with_suffix('.pending')
    with temp.open('xb') as f:f.write(raw);f.flush();os.fsync(f.fileno())
    os.rename(temp,p);require(json.loads(zlib.decompress(p.read_bytes()))==cp,'lossless state readback')
    rows=[*records,dict(file=name,sha256=sha(p),digest=cp['digest'],parent_digest=parent)]
    write(root/'head.json',dict(schema='prescribed_room_heater_history/v1',configuration_digest=digest(config),records=rows))
    require(sum(q.stat().st_size for q in (root/'states').iterdir())<512*1024**2,'room512MiBhistory guard')
    return cp,rows

def observe(config,result,revision):
    row=room_geometry.observations(config,result);row['revision']=revision
    require(abs(row['heat_defect_j'])<1e-8 and abs(row['smoke_defect_kg'])<1e-14,'room independent source/boundary accounting')
    require(row['external_work_j']==0 and abs(row['window_volume_rate_m3_s'])<1e-8,'room no imposed momentum and single-opening incompressible balance')
    return row

def export_frame(config,cp,directory):
    """Native VF3D with physical mask and explicit display-only wall cutaways."""
    from fire_render_diagnostic import plane,material
    from .playback_export import box_surfaces,make_render_request
    from .rig_scene_adapter import receiving_model
    import combined_scene
    directory=Path(directory);directory.mkdir()
    result=cp['fluid']
    with receiving_model(combined_scene,result):combined_scene.write_original_vf3d(result,directory,.0002)
    write(directory/'manifest.json',dict(manifest_version=2,frame_contract='vf3d',frames=[dict(frame_index=0,path='frame_000000.vf3d',frame_contract='vf3d',time_seconds=result['fields']['time_s'])]))
    objects=[plane('ground',[.5,.5,-.001],1.3,1.3),plane('heater',[.375,.5,.001],.25,.25)]
    mats=[material('ground',0x0B1620),material('heater',0xDE9A45)]
    for name,lo,hi in room_geometry.solid_boxes(config):
        if name in ('west','y_min','ceiling'):continue
        for obj in box_surfaces({'fluid':{'solid_box':dict(lower_m=lo,upper_m=hi)}},plane):
            obj['object_id']='room_'+name+'_'+obj['object_id'];objects.append(obj);mats.append(material(obj['object_id'],0x184653))
    scene=dict(schema_family='codework_scene',schema_variant='scene_runtime_v1',schema_version=1,scene_id='prescribed_single_window_room',space_mode_default='3d',unit_system='meters',world_scale=1.,objects=objects,lights=[dict(light_id='room_key',kind='point',radius=0,position=dict(x=.5,y=.9,z=1.3),intensity=4,falloff_distance=8,color=dict(r=.82,g=.94,b=1),enabled=True,moving=False)],extensions={'ray_tracing':{'authoring':{'object_materials':mats}},'room_diagnostic':dict(physics_checkpoint_digest=cp['digest'],display_cutaways=['west','y_min','ceiling'],source='prescribed heater and passive tracer; no Fire combustion')})
    request=make_render_request([.375,.5,0]);i=request['inspection'];i.update(camera_position=dict(x=-.8,y=-1.2,z=1.1),camera_look_at=dict(x=.5,y=.5,z=.4),camera_zoom=1.2,ambient_strength=.12,top_fill_strength=.15,background_brightness=.08,background_color=dict(r=.012,g=.035,b=.065))
    i['volume_smoke_optics'].update(mass_extinction_m2_kg=60000,scattering_albedo=.98,anisotropy=0.)
    write(directory/'playback_scene_runtime.json',scene);write(directory/'playback_render_request.json',request)
    return dict(revision=cp['revision'],time_s=result['fields']['time_s'],checkpoint_digest=cp['digest'],request=directory.name+'/playback_render_request.json',scene_sha256=sha(directory/'playback_scene_runtime.json'),request_sha256=sha(directory/'playback_render_request.json'),vf3d_sha256=sha(directory/'frame_000000.vf3d'))
