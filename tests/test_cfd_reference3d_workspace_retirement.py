"""Real backing lifetime and unchanged retained solution/load authority."""
import unittest,sys,gc
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_workspace_retirement import capture,verify
from cfd_reference3d_shared_factor import storage_sha

class Retirement(unittest.TestCase):
    def test_all_array_and_backing_release_with_live_solution_without_collection(self):
        enabled=gc.isenabled();counts=[s['collections'] for s in gc.get_stats()];a=np.arange(30.,dtype=float);b=a[4:12];live=(np.array([1.,2.,3.]),np.arange(8,dtype=np.int32));h=storage_sha(*live)
        refs,record=capture({'array':a,'view':b},0,1e-12);self.assertEqual(record['unique_backing_bytes'],a.nbytes)
        del a,b;row=verify(refs,record,live,h);self.assertTrue(row['all_completed_buffers_and_backing_owners_released'] and row['remaining_authority_preserved_bitwise']);self.assertFalse(row['explicit_collection_performed'] or row['allocator_relief_performed']);self.assertEqual(gc.isenabled(),enabled)
        np.testing.assert_array_equal(live[0],[1.,2.,3.])

    def test_retained_alias_base_and_mutated_solution_refused(self):
        a=np.arange(30.,dtype=float);view=a[4:12];alias=a[15:20];live=(np.arange(3.),);h=storage_sha(*live);refs,record=capture({'view':view},0,1e-12);del view,a
        with self.assertRaisesRegex(ValueError,'still live'):verify(refs,record,live,h)
        del alias;live[0][0]=1.
        with self.assertRaisesRegex(ValueError,'changed FE'):verify(refs,record,live,h)
        live[0][0]=0.;self.assertTrue(verify(refs,record,live,h)['remaining_authority_preserved_bitwise'])

    def test_unconverged_invalid_nonfinite_external_and_empty_authority_refused(self):
        a=np.arange(3.)
        for info,metric in ((1,1e-12),(0,2e-11),(0,-1e-12),(0,float('nan')),(0,float('inf'))):
            with self.assertRaises(ValueError):capture({'a':a},info,metric)
        for arrays in ({},{'a':a[::2]},{'a':np.full(3,np.nan)},{'a':np.array([object()],dtype=object)},{'a':np.frombuffer(bytearray(8),dtype=np.float64)}):
            with self.assertRaises(ValueError):capture(arrays,0,0.)
        refs,record=capture({'a':a},0,0.);del a
        with self.assertRaises(ValueError):verify(refs,record,(),storage_sha(np.arange(3.)))

if __name__=='__main__':unittest.main()
