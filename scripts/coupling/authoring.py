"""Read-only joint-head projection into the existing combined-scene authoring carrier."""
import json
from pathlib import Path
from surface_sources.growth_fire_v1 import sealed,require,digest


def export(host,output):
    cp=host.inspect();require(cp['revision']>0,'advance a complete joint window before export')
    source=cp['source'];result=cp['fluid'];frame=source['frame']
    # A derived snapshot, not a replacement solver checkpoint or mutable scene.
    projection=sealed({'schema':'physics_sim_offline_plume_checkpoint/v1','revision':cp['revision'],
        'time_s':result['fields']['time_s'],'result':result,'binding_digest':cp['binding_digest'],
        'joint_checkpoint_digest':cp['digest'],'consumed':[{'source_digest':frame['digest'],
            'event':frame['event'],'start_s':frame['interval']['start_s'],'end_s':frame['interval']['end_s'],
            'energy_j':frame['totals']['energy_transferred_j'],'smoke_kg':frame['totals']['smoke_transferred_kg']}]})
    import combined_scene
    require(Path(combined_scene.__file__).resolve()==Path(host.config['growth_root'])/'scripts/combined_scene.py',
            'scene exporter from a different checkout')
    output=Path(output)
    combined_scene.export(source,result,output,.0002,projection)
    (output/'joint_checkpoint.json').write_text(json.dumps(cp,allow_nan=False)+'\n')
    receipt=sealed({'schema':'fire_fluid_joint_scene_projection/v1','joint_checkpoint_digest':cp['digest'],
        'projection_digest':projection['digest'],'combined_scene_digest':json.loads((output/'combined_scene.json').read_text())['digest'],
        'purpose':'read_only_authoring_snapshot','physical_calibration_qualified':False})
    (output/'joint_projection.json').write_text(json.dumps(receipt,indent=2)+'\n')
    combined_scene.validate(output)
    require(json.loads((output/'joint_checkpoint.json').read_text())==cp and digest(cp)==receipt['joint_checkpoint_digest'],
            'joint scene readback')
    return receipt
