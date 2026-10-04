import sys,unittest,gc,weakref
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_residency as mod
class Owner:pass
class Residency(unittest.TestCase):
    def test_backing_views_deduplicated_without_copy(self):
        a=np.arange(100.,dtype=float);roots={'operator':{'view':a[10:70]},'factor':(a,a.reshape(10,10).T),'other':np.ones(5)};d=mod.array_owners(roots)
        self.assertEqual(d['known_unique_backing_bytes'],a.nbytes+40);self.assertEqual(d['known_unique_backing_owners'],2);self.assertEqual(d['groups']['operator']['attributed_backing_bytes'],a.nbytes);self.assertEqual(d['groups']['factor']['attributed_backing_bytes'],0);np.testing.assert_array_equal(a,np.arange(100.))
    def test_cycles_terminate_and_weak_refs_do_not_adopt_owners(self):
        a=np.ones(10);o=Owner();o.array=a;o.parent=o;d=mod.array_owners({'first':o,'alias':o,'weak':weakref.ref(o)})
        self.assertEqual(d['known_unique_backing_bytes'],80);self.assertEqual(d['groups']['alias']['attributed_backing_bytes'],0)
    def test_fresh_residency_can_refuse_stale_admission_without_reducing_reserve(self):
        def old(f,b):return dict(current_rss_before_numeric_bytes=100,factor_storage_bytes=1800*2**20-32*2**20-3210,numeric_workspace_bytes=10,reserve_bytes=32*2**20,basis_reservation_bytes=b,estimated_numeric_stage_bytes=100+1800*2**20-32*2**20-3210+10+32*2**20+b,numeric_stage_admitted=True)
        with patch.object(mod,'numeric_stage_admission',side_effect=old):
            d=mod.fresh_admission(None,1000,2000,lambda:500);self.assertEqual(d['current_rss_before_numeric_bytes'],500);self.assertEqual(d['earlier_post_relief_rss_bytes'],100);self.assertEqual(d['basis_reservation_bytes'],3000);self.assertEqual(d['reserve_bytes'],32*2**20);self.assertLessEqual(d['earlier_post_relief_estimate_bytes'],1800*2**20);self.assertFalse(d['numeric_stage_admitted']);self.assertEqual(d['estimated_numeric_stage_bytes']-d['earlier_post_relief_estimate_bytes'],400)
            with self.assertRaises(ValueError):mod.fresh_admission(None,1,2,lambda:0)
    def test_actual_disposed_mesh_mapping_cycle_needs_collection(self):
        enabled=gc.isenabled();gc.disable()
        try:
            m=MeshTet.init_tensor([0,1],[0,1],[0,1]);mapping=m.mapping();self.assertIs(mapping.mesh,m);r=weakref.ref(m);q=weakref.ref(mapping);del m,mapping
            self.assertIsNotNone(r());self.assertIsNotNone(q());gc.collect();self.assertIsNone(r());self.assertIsNone(q())
        finally:
            if enabled:gc.enable()
if __name__=='__main__':unittest.main()
