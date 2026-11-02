"""Fine native Fire texture with same-clock CFD volume and optional cutaway walls."""
import json,math
from pathlib import Path
from surface_sources.growth_fire_v1 import require


def render_inputs(config,directory,bundle,seed_fuel):
    from fire_ground_scene import prepare
    from fire_render_diagnostic import plane,material
    from combined_scene import save
    from .playback_export import box_surfaces,make_render_request
    from .ventilation_scene import cutaway_surfaces
    directory=Path(directory);assets=directory/'appearance';assets.mkdir(exist_ok=False)
    appearance=prepare(bundle,seed_fuel,assets,640,480)
    cp=json.loads((directory/'joint_checkpoint.json').read_text())
    require(appearance['source_digest']==cp['source']['digest'] and math.isclose(appearance['time_s'],cp['fluid']['fields']['time_s'],abs_tol=1e-12),'fine surface/volume source and time identity')
    scene=json.loads((assets/'scene_runtime.json').read_text());mats=scene['extensions']['ray_tracing']['authoring']['object_materials']
    for m in mats:
        if 'authored_texture' in m:m['authored_texture']['manifest_path']='appearance/'+m['authored_texture']['manifest_path']
    objects=box_surfaces(config,plane);walls=cutaway_surfaces(config,plane)
    if 'rig_geometry' in config['fluid']:
        from .rig_geometry import render_surfaces
        walls=render_surfaces(config['fluid'],plane)
    scene['objects'].extend(objects+walls);mats.extend(material(o['object_id'],0x8A939E) for o in objects);mats.extend(material(o['object_id'],0xC6CBD1) for o in walls)
    save(directory/'playback_scene_runtime.json',scene)
    request=make_render_request([.5,.5,0]);request['inspection'].update(camera_position={'x':1.5,'y':-1.2,'z':.85},camera_look_at={'x':.5,'y':.5,'z':.35},ambient_strength=.3,background_brightness=1.7,background_color={'r':.82,'g':.91,'b':1})
    request['resources']={'max_workers':4,'cpu_percent':70,'reserve_cpu_count':2}
    save(directory/'playback_render_request.json',request)
    save(directory/'appearance-projection.json',{'source_digest':appearance['source_digest'],'time_s':appearance['time_s'],'texture_sha256':appearance['texture_sha256'],'representation':'native texture and CFD volume; channel is a display cutaway, omitted walls remain closed in solver','cutaway_wall_objects':[o['object_id'] for o in walls], 'rig_geometry':config['fluid'].get('rig_geometry'), 'physics_closed_cutaways':['y_min','x_max','ceiling'] if 'rig_geometry' in config['fluid'] else []})
