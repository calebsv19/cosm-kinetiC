"""Workbench preset admission and same-clock fine surface export."""
import json,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from coupling.workbench_wind import defaults,make_config,valid_settings
from coupling.wind_presets import PROFILES,configuration
from coupling.workbench import Workbench
class WindWorkbenchTests(unittest.TestCase):
 def setUp(self):
  self.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
 def test_qualified_defaults_and_admission(self):
  for p in PROFILES:self.assertEqual(make_config(self.workers,defaults(p)),configuration(self.workers,p))
  for change in ({'exchange_s':.05},{'gust_start_s':.401},{'duration_s':6.4,'exchange_s':.02}):
   with self.assertRaises(ValueError):valid_settings(dict(defaults('abrupt_gust'),**change))
 def test_fine_export_resume_and_texture_refusal(self):
  with tempfile.TemporaryDirectory() as t,patch.dict(os.environ,{'PHYSICS_SIM_EXPERIMENT_ROOT':t+'/evidence'}):
   w=Workbench(Path(t)/'panel',self.workers)
   try:
    w.create(dict(defaults('channel_patch'),duration_s=.3))
    for i in range(3):w.step(i,'wind-'+str(i))
    state=w.state();self.assertTrue(state['complete']);self.assertIn('channel_patch',state['wind_presets'])
    reopened=Workbench(Path(t)/'panel',self.workers)
    try:self.assertEqual(state,reopened.state())
    finally:reopened.close()
    exported=w.export(revisions=[3]);self.assertEqual(len(exported['manifest']['frames']),1)
    row=exported['manifest']['frames'][0];self.assertEqual(row['revision'],3)
    folder=Path(exported['output'])/Path(row['request']).parent
    from combined_scene import validate
    self.assertAlmostEqual(validate(folder)['snapshot']['time_s'],.3)
    appearance=json.loads((folder/'appearance-projection.json').read_text());self.assertAlmostEqual(appearance['time_s'],.3)
    self.assertTrue(appearance['cutaway_wall_objects'])
    with (folder/'appearance/burn-field.png').open('ab') as f:f.write(b'tamper')
    from coupling.render_preview import render
    with patch('coupling.render_preview.subprocess.run') as call:
     with self.assertRaisesRegex(ValueError,'texture/clock'):render(exported['output'],self.workers[1],[3])
     call.assert_not_called()
   finally:w.close()
if __name__=='__main__':unittest.main()
