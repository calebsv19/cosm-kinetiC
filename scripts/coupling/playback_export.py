"""Same-clock native snapshots and bounded existing-RayTracing render inputs."""
import json
from pathlib import Path
from types import SimpleNamespace
from .authoring import export
from .workbench import read_frames
from surface_sources.growth_fire_v1 import require, sealed


def box_surfaces(config, plane):
    box = config['fluid'].get('solid_box')
    if box is None: return []
    lo, hi = box['lower_m'], box['upper_m']
    center = [(a + b) / 2 for a, b in zip(lo, hi)]
    size = [b - a for a, b in zip(lo, hi)]
    result = []
    for axis, u, v in ((0, (0, 1, 0), (0, 0, 1)), (1, (1, 0, 0), (0, 0, 1)), (2, (1, 0, 0), (0, 1, 0))):
        remaining = [a for a in range(3) if a != axis]
        for side, position in (('lower', lo[axis]), ('upper', hi[axis])):
            point = center.copy(); point[axis] = position
            # Keep right-handed surface frames on both sides of the box.
            normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
            desired = -1 if side == 'lower' else 1
            face_v = v
            if normal[axis] != desired:
                face_v = tuple(-x for x in v); normal = [-x for x in normal]
            result.append(plane('inert_box_' + str(axis) + '_' + side, point, size[remaining[0]], size[remaining[1]], u, face_v, normal))
    return result


def export_playback(host, output, revisions=None):
    frames = read_frames(host)
    require(len(frames) > 1, 'advance an exchange before exporting')
    if revisions is not None:
        require(type(revisions) is list and revisions and len(set(revisions)) == len(revisions), 'distinct selected revisions')
        require(all(type(v) is int and 0 < v <= frames[-1]['revision'] for v in revisions), 'accepted export revision')
    output = Path(output); output.mkdir(parents=True, exist_ok=False)
    from fire_render_diagnostic import surface_patches, plane, material
    from combined_scene import save, sha
    rows = []
    for cp in frames[1:]:
        if revisions is not None and cp['revision'] not in revisions: continue
        directory = output / ('revision-%03d' % cp['revision'])
        receipt = export(SimpleNamespace(config=host.config, inspect=lambda cp=cp: cp), directory)
        render_inputs(host.config,directory)
        rows.append({'revision': cp['revision'], 'time_s': cp['fluid']['fields']['time_s'],
                     'checkpoint_digest': cp['digest'], 'joint_projection_digest': receipt['digest'],
                     'request': str(directory.relative_to(output) / 'playback_render_request.json'),
                     'scene_sha256': sha(directory / 'playback_scene_runtime.json'),
                     'request_sha256': sha(directory / 'playback_render_request.json'),
                     'vf3d_sha256': sha(directory / 'frame_000000.vf3d')})
    value = sealed({'schema': 'fire_fluid_playback_export/v1', 'frames': rows,
                    'head_checkpoint_digest': frames[-1]['digest'], 'feedback_enabled': 'return_channel' in host.config,
                    'surface_mapping': 'clipped dual-cell fuel/burn/ash palette; no airborne proxy geometry',
                    'volume_mapping': 'accepted native smoke concentration; cell-centred optical candidate, no density amplification',
                    'limits': ['Fire heat is an ignition proxy, not temperature.', 'No flame geometry, airborne particles or simulated radiation.',
                               'No thermal return or physical calibration.', 'No latest-good scene promotion.']})
    save(output / 'playback.json', value)
    return {'output': str(output), 'manifest': value}


def render_inputs(config,directory):
    """Render native accepted fields, whether coupled or prescribed-source qualification."""
    from fire_render_diagnostic import surface_patches,plane,material
    from combined_scene import save
    surface = json.loads((directory / 'surface_fields.json').read_text())
    objects, materials = surface_patches(surface, True)
    # Surface appearance only: diagnostic emission cards are not flame geometry.
    objects = [obj for obj in objects if not obj['object_id'].startswith('fire_emitter_')]
    materials = [mat for mat in materials if not mat['object_id'].startswith('fire_emitter_')]
    solid_objects = box_surfaces(config, plane)
    materials.extend(material(obj['object_id'], 0x8A939E) for obj in solid_objects)
    source=config['source'];center=[source['origin_m'][0]+(source['width']-1)*source['cell_size_m']/2,source['origin_m'][1]+(source['height']-1)*source['cell_size_m']/2,0]
    scene = {'schema_family': 'codework_scene', 'schema_variant': 'scene_runtime_v1', 'schema_version': 1,
             'scene_id': 'fire_fluid_coupled_playback', 'space_mode_default': '3d', 'unit_system': 'meters', 'world_scale': 1.,
             'objects': [plane('receiver_floor', [config['fluid']['length_m'][0]/2,config['fluid']['length_m'][1]/2,-.001], config['fluid']['length_m'][0]*1.1,config['fluid']['length_m'][1]*1.1), *objects, *solid_objects],
             'lights': [{'light_id': 'review_light', 'kind': 'sphere', 'position': {'x': -1., 'y': -1., 'z': 2.},
                         'radius': .01, 'intensity': 3, 'falloff_distance': 10, 'color': {'r': 1, 'g': .7, 'b': .4}, 'enabled': True, 'moving': True}],
             'extensions': {'ray_tracing': {'authoring': {'object_materials': [material('receiver_floor', 0x707884), *materials]}}}}
    save(directory / 'playback_scene_runtime.json', scene)
    request = {'schema_version': 'ray_tracing_agent_render_request_v1', 'run_id': 'accepted_native_snapshot',
               'scene': {'runtime_scene_path': 'playback_scene_runtime.json'},
               'volume': {'enabled': True, 'source_kind': 'manifest', 'source_path': 'manifest.json', 'visible': True, 'affects_lighting': True, 'sampling_layout': 'cell_centered'},
               'render': {'start_frame': 0, 'frame_count': 1, 'width': 640, 'height': 480, 'temporal_frames': 1,
                          'integrator_3d': 'direct_light', 'denoise_enabled': False},
               'inspection': {'camera_position': {'x': 1.05, 'y': -.7, 'z': .35},
                              'camera_look_at': {'x': center[0], 'y': center[1], 'z': .25},
                              'environment_light_mode': 'ambient', 'ambient_strength': .35, 'background_brightness': .35,
                              'top_fill_strength': 0, 'object_audit_max_dimension': 640,
                              'volume_step_scale': 1,
                              'volume_smoke_optics': {'schema': 'physical_smoke_v1',
                                  'reference_concentration_kg_m3': .0002, 'mass_extinction_m2_kg': 7500,
                                  'scattering_albedo': .65, 'anisotropy': .35,
                                  'sky_illumination': True, 'sky_error_budget': .00001}},
               'resources': {'max_workers': 1}, 'output': {'root': 'preview', 'overwrite': False}}
    save(directory / 'playback_render_request.json', request)
