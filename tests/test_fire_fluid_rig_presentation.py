import sys,json,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import rig_presentation as view
from surface_sources.growth_fire_v1 import sealed

class ViewTests(unittest.TestCase):
    def test_explicit_supported_edits_only(self):
        value=dict(schema=view.SCHEMA,preset=view.PRESET,parent_playback_digest='a'*64)
        self.assertEqual(view.parse_edit(value),value)
        for bad in [dict(value,camera_position={}),dict(value,preset='arbitrary'),dict(value,parent_playback_digest='wrong')]:
            with self.assertRaises(ValueError):view.parse_edit(bad)
    def test_only_declared_objects_and_camera_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp).resolve();d=root/'one';d.mkdir()
            ids=['rig_west_'+str(i) for i in range(24)]+['fire_surface','rig_partition']
            scene=dict(objects=[dict(object_id=i) for i in ids],extensions=dict(ray_tracing=dict(authoring=dict(object_materials=[dict(object_id=i) for i in ids]))))
            request=dict(schema_version='ray_tracing_agent_render_request_v1',inspection={},volume=dict(source_path='manifest.json',visible=True))
            view.save(d/'playback_scene_runtime.json',scene);view.save(d/'request.json',request)
            m=sealed(dict(schema='fire_fluid_playback_export/v1',frames=[dict(request='one/request.json',scene_sha256=view.sha(d/'playback_scene_runtime.json'),request_sha256=view.sha(d/'request.json'))]))
            view.save(root/'playback.json',m);result=view.apply_export(root,dict(schema=view.SCHEMA,preset=view.PRESET,parent_playback_digest=m['digest']))
            self.assertEqual(json.loads((d/'request.json').read_text())['volume'],request['volume'])
            self.assertEqual([o['object_id'] for o in json.loads((d/'playback_scene_runtime.json').read_text())['objects']],['fire_surface','rig_partition'])
            self.assertTrue(result['presentation']['physics_skipped'])
            repeated=view.apply_export(root,dict(schema=view.SCHEMA,preset=view.PRESET,parent_playback_digest=result['digest']));self.assertEqual(result,repeated)

if __name__=='__main__':unittest.main()
