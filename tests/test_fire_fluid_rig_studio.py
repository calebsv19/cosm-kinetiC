import sys,json,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import rig_studio as view
from surface_sources.growth_fire_v1 import sealed
class StudioTests(unittest.TestCase):
 def make(self,root):
  d=root/'one';d.mkdir();(d/'frame_000000.vf3d').write_bytes(b'exact accepted density')
  req=dict(schema_version='ray_tracing_agent_render_request_v1',inspection=dict(volume_smoke_optics=dict(schema='physical_smoke_v1',mass_extinction_m2_kg=7500)),volume=dict(source_path='manifest.json'),render=dict(width=640,height=480))
  scene=dict(objects=[dict(object_id='ground',primitive=dict(frame=dict(origin=dict(x=1,y=1,z=0))),transform=dict(position=dict(x=1,y=1,z=0))),dict(object_id='fire_surface'),dict(object_id='rig_partition'),dict(object_id='marker_0')],lights=[dict(light_id='sky_key',intensity=2)],extensions=dict(ray_tracing=dict(authoring=dict(object_materials=[dict(object_id=i,object_color=1) for i in ['ground','fire_surface','rig_partition','marker_0']]))))
  view.save(d/'request.json',req);view.save(d/'playback_scene_runtime.json',scene)
  m=sealed(dict(frames=[dict(request='one/request.json',request_sha256=view.sha(d/'request.json'),scene_sha256=view.sha(d/'playback_scene_runtime.json'),vf3d_sha256=view.sha(d/'frame_000000.vf3d'))]));view.save(root/'playback.json',m)
  return d,req,m,dict(schema=view.SCHEMA,preset=view.PRESET,parent_playback_digest=m['digest'])
 def test_style_preserves_density_optics_source_and_render_contract(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);d,q,m,e=self.make(root);before=(d/'frame_000000.vf3d').read_bytes();out=view.apply(root,e);after=json.loads((d/'request.json').read_text());scene=json.loads((d/'playback_scene_runtime.json').read_text())
   self.assertEqual(before,(d/'frame_000000.vf3d').read_bytes());self.assertEqual(after['volume'],q['volume']);self.assertEqual(after['render'],q['render']);self.assertEqual(after['inspection']['volume_smoke_optics']['mass_extinction_m2_kg'],60000);self.assertEqual(q['inspection']['volume_smoke_optics']['mass_extinction_m2_kg'],7500);self.assertEqual(next(v for v in scene['extensions']['ray_tracing']['authoring']['object_materials'] if v['object_id']=='fire_surface')['object_color'],1);self.assertTrue(out['studio_presentation']['physics_skipped'])
 def test_tampered_density_refused_before_request_write(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);d,q,m,e=self.make(root);before=(d/'request.json').read_bytes();(d/'frame_000000.vf3d').write_bytes(b'drift')
   with self.assertRaises(ValueError):view.apply(root,e)
   self.assertEqual(before,(d/'request.json').read_bytes())
if __name__=='__main__':unittest.main()
