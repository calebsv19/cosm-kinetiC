"""Actual native solid-floor continuation, rollback, and bound return sampling."""
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.framework import Framework
from coupling.scenario import baseline
from coupling.fluid_adapter import conservation
from coupling.velocity import observation
from fire_fluid_spatial import ground_configuration
class GroundTests(unittest.TestCase):
    def test_restart_replay_and_failed_staging(self):
        with tempfile.TemporaryDirectory(prefix='fire-fluid-ground-') as temporary:
            root=Path(temporary);prior=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT')
            os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(root/'evidence')
            try:
                cfg=ground_configuration(baseline(os.environ['FIRE_FLUID_GROWTH_ROOT'],os.environ['FIRE_FLUID_FIRE_WORKER'],os.environ['FIRE_FLUID_OPEN_WORKER'],os.environ['FIRE_FLUID_CLOCK_WORKER']))
                host=Framework(cfg,root/'run');host.initialize();first=host.step(0,'first')
                before=host.path.read_bytes();self.assertEqual(host.step(0,'first'),first);self.assertEqual(before,host.path.read_bytes())
                def fail(cp):raise RuntimeError('staged')
                with self.assertRaises(RuntimeError):host.step(1,'second',before_publish=fail)
                self.assertEqual(before,host.path.read_bytes())
                (root/'restart').mkdir();shutil.copy2(host.path,root/'restart/joint.sqlite3')
                other=Framework(cfg,root/'restart')
                self.assertEqual(host.step(1,'second'),other.step(1,'second'))
                cp=host.inspect();self.assertEqual(cp,other.inspect())
                value=observation(cp,cfg,16,0,height_m=.0625)
                self.assertEqual(value['joint_checkpoint_digest'],cp['digest'])
                self.assertFalse(value['applied_to_fire'])
                with self.assertRaises(ValueError):observation(cp,cfg,17,0,height_m=.0625)
            finally:
                if prior is None:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None)
                else:os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=prior
    def test_reservoir_flux_is_independently_included(self):
        cfg={'grid':[1,1,1],'boundary_policy':{}}
        data={'energy_j':[5.],'smoke_kg':[10.],'initial_energy_j':1.,'initial_smoke_kg':2.,
              'input_energy_j':3.,'input_smoke_kg':6.,'boundary_fluxes':[2.,0.,1.,0.,4.,0.,2.,0.]}
        result={'state':{'data':data},'fields':{'energy_j':[5.],'smoke_kg':[10.]}}
        conservation(cfg,result)
        data['boundary_fluxes'][2]=2.
        with self.assertRaises(ValueError):conservation(cfg,result)
if __name__=='__main__':unittest.main()
