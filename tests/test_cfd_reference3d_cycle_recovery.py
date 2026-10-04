import sys,unittest,gc,weakref
from pathlib import Path
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_cycle_recovery import collect_live
class CycleRecovery(unittest.TestCase):
    def test_exact_live_action_state_and_unreachable_mesh_reclaimed(self):
        enabled=gc.isenabled();gc.disable()
        try:
            m=MeshTet.init_tensor([0,1],[0,1],[0,1]);mapping=m.mapping();r=weakref.ref(m);q=weakref.ref(mapping);del m,mapping
            self.assertIsNotNone(r());a=np.arange(20.,dtype=float);values=iter((1000,800));d=collect_live((a,),lambda:2*a,rss_reader=lambda:next(values))
            self.assertIsNone(r());self.assertIsNone(q());self.assertGreater(d['collected_objects'],0);self.assertTrue(d['live_input_preserved_bitwise']);self.assertTrue(d['full_action_preserved_bitwise']);self.assertFalse(d['gc_enabled_after']);self.assertEqual(d['current_rss_after_validation_bytes'],800);np.testing.assert_array_equal(a,np.arange(20.))
        finally:
            if enabled:gc.enable()
    def test_live_mutation_refused(self):
        a=np.ones(3)
        def bad():a[1]=2;return 0
        with self.assertRaisesRegex(ValueError,'input bits'):collect_live((a,),lambda:a.copy(),collector=bad,rss_reader=lambda:100)
    def test_signed_zero_action_change_refused(self):
        a=np.ones(2);count=[0]
        def action():count[0]+=1;return np.array([0. if count[0]==1 else -0.])
        with self.assertRaisesRegex(ValueError,'action bits'):collect_live((a,),action,collector=lambda:0,rss_reader=lambda:100)
    def test_invalid_inputs_and_counts_refused(self):
        with self.assertRaises(ValueError):collect_live((),lambda:np.zeros(1))
        with self.assertRaises(ValueError):collect_live((np.arange(10.)[::2],),lambda:np.zeros(1))
        with self.assertRaises(ValueError):collect_live((np.ones(2),),lambda:np.zeros(1),collector=lambda:-1,rss_reader=lambda:100)
if __name__=='__main__':unittest.main()
