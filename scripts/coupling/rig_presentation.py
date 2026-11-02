"""Typed diagnostic camera/cutaway edit; accepted simulation fields are immutable."""
import hashlib,json,re
from pathlib import Path
from surface_sources.growth_fire_v1 import require,sealed
SCHEMA='fire_fluid_rig_view_edit/v1'
PRESET='upstream_cutaway/v1'
POSITION={'x':-.8,'y':-1.2,'z':1.1}
LOOK_AT={'x':.5,'y':.5,'z':.3}


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(path,value):path.write_text(json.dumps(value,sort_keys=True,allow_nan=False)+'\n')


def parse_edit(value):
    require(set(value)=={'schema','preset','parent_playback_digest'} and value['schema']==SCHEMA and value['preset']==PRESET,'supported typed rig view edit')
    require(type(value['parent_playback_digest']) is str and re.fullmatch('[0-9a-f]{64}',value['parent_playback_digest']),'explicit view parent')
    return dict(value)


def apply_export(root,edit):
    edit=parse_edit(edit);root=Path(root);manifest=json.loads((root/'playback.json').read_text())
    require(manifest==sealed(manifest) and manifest['digest']==edit['parent_playback_digest'],'rig view parent identity')
    if manifest.get('presentation',{}).get('preset')==PRESET:return manifest
    prepared=[]
    for row in manifest['frames']:
        request_path=root/row['request'];scene_path=request_path.parent/'playback_scene_runtime.json'
        require(request_path.resolve().is_relative_to(root.resolve()) and not request_path.is_symlink() and not scene_path.is_symlink(),'view payload containment')
        require(sha(request_path)==row['request_sha256'] and sha(scene_path)==row['scene_sha256'],'parent view inputs changed')
        request=json.loads(request_path.read_text());scene=json.loads(scene_path.read_text())
        require(request['schema_version']=='ray_tracing_agent_render_request_v1','view request type')
        deleted={o['object_id'] for o in scene['objects'] if o['object_id'].startswith('rig_west_')}
        require(len(deleted)==24,'fixed rig west cutaway object family')
        scene['objects']=[o for o in scene['objects'] if o['object_id'] not in deleted]
        authoring=scene['extensions']['ray_tracing']['authoring'];authoring['object_materials']=[m for m in authoring['object_materials'] if m['object_id'] not in deleted]
        request['inspection'].update(camera_position=POSITION,camera_look_at=LOOK_AT)
        scene['extensions'].setdefault('scene_iteration',{})['diagnostic_view']=dict(schema=SCHEMA,preset=PRESET,parent_playback_digest=edit['parent_playback_digest'],physics_closed_cutaways=['x_min','x_max','y_min','ceiling'])
        appearance_path=request_path.parent/'appearance-projection.json';appearance=None
        if 'appearance_sha256' in row:
            require(sha(appearance_path)==row['appearance_sha256'],'parent appearance metadata changed')
            appearance=json.loads(appearance_path.read_text())
            appearance.update(physics_closed_cutaways=['x_min','x_max','y_min','ceiling'],diagnostic_view_preset=PRESET)
        prepared.append((row,request_path,scene_path,request,scene,appearance_path,appearance))
    # Validate the complete batch before the first derived-file write.
    for row,request_path,scene_path,request,scene,appearance_path,appearance in prepared:
        save(request_path,request);save(scene_path,scene)
        row.update(request_sha256=sha(request_path),scene_sha256=sha(scene_path))
        if appearance is not None:
            save(appearance_path,appearance);row['appearance_sha256']=sha(appearance_path)
    manifest['presentation']=dict(edit,camera_position=POSITION,camera_look_at=LOOK_AT,physics_skipped=True,field_density_mapping_unchanged=True,physics_closed_cutaways=['x_min','x_max','y_min','ceiling'])
    manifest=sealed(manifest);save(root/'playback.json',manifest);return manifest
