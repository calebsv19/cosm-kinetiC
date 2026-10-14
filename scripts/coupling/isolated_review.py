"""Fine matched-source native snapshots; never rebind a coupled journal."""
import json
from pathlib import Path
from .framework import load_fire
from .playback_export import render_inputs
from surface_sources.growth_fire_v1 import sealed

def export(root,growth,names=('no-box','box')):
    root=Path(root);load_fire(growth)
    from combined_scene import export as export_scene,validate
    source=json.loads((root/'no-box/head.json').read_text())['source'];frame=source['frame']
    outputs=[]
    for name in names:
        directory=root/('fine-review-'+name)
        config=json.loads((root/('matched-'+name+'-32/config.json')).read_text())
        result=json.loads((root/('matched-'+name+'-32/fluid.json')).read_text())
        projection=sealed({'schema':'physics_sim_offline_plume_checkpoint/v1','revision':32,'time_s':result['fields']['time_s'],'result':result,
            'purpose':'matched prescribed Fire source, native 32-cubed Fluid; not a live coupled journal',
            'consumed':[{'source_digest':frame['digest'],'event':frame['event'],'end_s':frame['interval']['end_s']}]})
        export_scene(source,result,directory,.0002,projection);validate(directory);render_inputs(config,directory)
        outputs.append(directory/'playback_render_request.json')
    return outputs
