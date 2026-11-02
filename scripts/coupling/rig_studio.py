"""Typed dark-studio presentation of retained fields; no numerical mutation."""
import json,re,hashlib
from pathlib import Path
from surface_sources.growth_fire_v1 import require,sealed
SCHEMA='fire_fluid_rig_studio_edit/v1'
PRESET='dark_teal_keylight/v4'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):Path(p).write_text(json.dumps(v,sort_keys=True,allow_nan=False)+'\n')
def apply(root,edit):
    require(set(edit)=={'schema','preset','parent_playback_digest'} and edit['schema']==SCHEMA and edit['preset']==PRESET,'supported studio edit')
    require(type(edit['parent_playback_digest']) is str and re.fullmatch('[0-9a-f]{64}',edit['parent_playback_digest']),'explicit parent required')
    root=Path(root).resolve();m=json.loads((root/'playback.json').read_text())
    require(m==sealed(m) and m['digest']==edit['parent_playback_digest'],'studio parent changed')
    prepared=[]
    for row in m['frames']:
        q=root/row['request'];s=q.parent/'playback_scene_runtime.json'
        require(q.resolve().is_relative_to(root) and s.resolve().is_relative_to(root) and not q.is_symlink() and not s.is_symlink(),'studio containment')
        require(sha(q)==row['request_sha256'] and sha(s)==row['scene_sha256'] and sha(q.parent/'frame_000000.vf3d')==row['vf3d_sha256'],'parent inputs changed')
        request=json.loads(q.read_text());scene=json.loads(s.read_text())
        require(request['schema_version']=='ray_tracing_agent_render_request_v1','studio request type')
        i=request['inspection'];i.update(camera_position={'x':-.8,'y':-1.2,'z':1.1},camera_look_at={'x':.5,'y':.5,'z':.4},camera_zoom=1.2,ambient_strength=.12,top_fill_strength=.15,background_brightness=.08,background_color={'r':.012,'g':.035,'b':.065})
        # Authored eightfold optical extinction improves display visibility; density is unchanged.
        require('volume_smoke_optics' in i,'physical smoke contract required')
        i['volume_smoke_optics'].update(mass_extinction_m2_kg=60000,scattering_albedo=.98,anisotropy=0.)
        for o in scene['objects']:
            if o['object_id']=='ground':
                o['primitive'].update(width=1.3,height=1.3)
                o['transform']['position'].update(x=.5,y=.5)
                o['primitive']['frame']['origin'].update(x=.5,y=.5)
        deleted={o['object_id'] for o in scene['objects'] if o['object_id'] in ('marker_0','marker_1') or o['object_id'].startswith('rig_y_max_')}
        scene['objects']=[o for o in scene['objects'] if o['object_id'] not in deleted]
        mats=scene['extensions']['ray_tracing']['authoring']['object_materials']
        mats[:]=[v for v in mats if v['object_id'] not in deleted]
        for v in mats:
            if v['object_id']=='ground':v.update(object_color=0x0B1620,roughness=.95)
            elif v['object_id'].startswith('rig_'):v.update(object_color=0x184653,roughness=.85)
        keys=[v for v in scene['lights'] if v['light_id']=='sky_key'];require(len(keys)==1,'existing native keylight required')
        keys[0].update(position={'x':.5,'y':.9,'z':1.3},intensity=4,falloff_distance=8,color={'r':.82,'g':.94,'b':1})
        scene['lights'].append(dict(light_id='studio_window_fill',kind='point',radius=0,position={'x':-.8,'y':-.8,'z':1.1},intensity=1.5,falloff_distance=6,color={'r':1.,'g':.82,'b':.63},enabled=True))
        scene['extensions'].setdefault('scene_iteration',{})['studio_edit']=dict(edit,physics_skipped=True)
        prepared.append((row,q,s,request,scene))
    for row,q,s,request,scene in prepared:
        save(q,request);save(s,scene);row.update(request_sha256=sha(q),scene_sha256=sha(s))
    m['studio_presentation']=dict(edit,physics_skipped=True,density_bytes_unchanged=True,optical_mapping_unchanged=False,display_extinction_multiplier=8,cutaway_back_wall=True)
    m=sealed(m);save(root/'playback.json',m);return m
