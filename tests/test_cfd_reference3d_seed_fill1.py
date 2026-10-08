import gc,json,sys,unittest,weakref
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
LIB=library_path('build/c3d-seed-fill1/support/factor.dylib')
from cfd_reference3d_bounded_fill1 import BlockIC0 as Seed
from cfd_reference3d_seed_fill1_block import SeedInformedBlockIC0
from cfd_reference3d_seed_fill1_graph import seed_pattern
from test_cfd_reference3d_priority_fill1 import fixture
from test_cfd_reference3d_bounded_fill1 import dense_lower

def oracle(seed):
    n=seed.owner.nodes;physical=[set() for _ in range(n)];t=seed.owner;values=seed.rounded.reshape(-1,3,3).astype(float)
    diagonal=np.sqrt([np.linalg.norm(values[t.starts[i]]) for i in seed.perm]);lower=seed.lower_copy().reshape(-1,3,3);scores=[{} for _ in range(n)]
    for i in range(n):
        for k in range(t.starts[i],t.starts[i+1]):a,b=sorted((int(seed.inverse_permutation[i]),int(seed.inverse_permutation[t.rows[k]])));physical[a].add(b)
    for col in range(n):
        positions=list(range(seed.starts[col]+1,seed.starts[col+1]))
        for q,ki in enumerate(positions):
            i=seed.rows[ki];wi=np.linalg.norm(lower[ki])/diagonal[i]
            for kj in positions[q+1:]:
                j=seed.rows[kj];scores[i][j]=scores[i].get(j,0.)+wi*np.linalg.norm(lower[kj])/diagonal[j]
    starts=[0];rows=[];generated=kept=0
    for i in range(n):
        extra={j:v for j,v in scores[i].items() if j not in physical[i]};picked=sorted(extra,key=lambda j:(-extra[j],j))[:len(physical[i])-1]
        generated+=len(extra);kept+=len(picked);rows.extend(sorted(physical[i]|set(picked)));starts.append(len(rows))
    return np.array(starts),np.array(rows),generated,kept
class Informed(unittest.TestCase):
    def test_independent_seed_graph_oracle_caps_input_preservation(self):
        for equal in (False,True):
            t=fixture(equal);seed=Seed(t,LIB,False);before=seed.lower_copy();a,b,generated,kept=oracle(seed);st,rr,p,inv,m=seed_pattern(seed,LIB,0)
            np.testing.assert_array_equal(st,a);np.testing.assert_array_equal(rr,b);np.testing.assert_array_equal(seed.lower_copy(),before)
            self.assertEqual(m['generated_fill'],generated);self.assertEqual(m['kept_fill'],kept);self.assertLessEqual(len(rr),m['pattern_block_cap']);self.assertTrue(seed.input_unchanged());seed.close()
    def test_final_factor_inverse_positive_error_model_seed_retirement(self):
        t=fixture();f=SeedInformedBlockIC0(t,LIB,False);L,p=dense_lower(f);model=(L@L.T)[np.ix_(p,p)];x=np.random.default_rng(1921).normal(size=f.n)
        np.testing.assert_allclose(f.solve(x),np.linalg.solve(model,x),rtol=1e-10,atol=1e-10)
        rounded=np.zeros_like(model)
        for j in range(t.nodes):
            for k in range(t.starts[j],t.starts[j+1]):
                i=t.rows[k];U=f.rounded.reshape(-1,3,3)[k].astype(float);ix=np.arange(3)*t.nodes+j;iy=np.arange(3)*t.nodes+i;rounded[np.ix_(ix,iy)]=U;rounded[np.ix_(iy,ix)]=U.T
        self.assertGreater(np.linalg.eigvalsh(model).min(),0);self.assertGreaterEqual(np.linalg.eigvalsh(model-rounded).min(),-1e-10)
        self.assertTrue(f.input_unchanged());self.assertTrue(f.metadata['seed_handle_retired_before_final_numeric']);self.assertTrue(f.metadata['seed_graph_inputs_retired_before_final_numeric']);self.assertEqual(f.metadata['shifted_pivots'],0);f.close()
    def test_actual_anisotropic_FE_pressure_and_all_owners(self):
        from test_cfd_reference3d_distributed_p3_cg8 import fixture as actual
        from cfd_reference3d_seed_fill1_pressure import SeedFill1PressureFactor
        from cfd_reference3d_vector_storage import VectorTriangle
        _,s=actual();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LIB);f=SeedFill1PressureFactor(t,LIB,pressure_control=False,coarse_library=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib'),Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
        x,y=np.random.default_rng(1922).normal(size=(2,nv));px,py=f.pressure_solve(x),f.pressure_solve(y);self.assertGreater(x@px,0);self.assertGreater(x@f.solve(x),0);np.testing.assert_allclose(x@py,y@px,rtol=1e-10,atol=1e-7)
        self.assertTrue(f.input_unchanged());refs=[weakref.ref(f.starts),weakref.ref(f.linear_pressure),weakref.ref(f.balanced),weakref.ref(f.coarse_factor.starts)];f.close();del f;gc.collect();self.assertTrue(all(r() is None for r in refs))
    def test_graph_budget_rejection_keeps_seed_and_constructor_reservations(self):
        from cfd_reference3d_domain_budget import PhaseResourceStopped
        t=fixture();seed=Seed(t,LIB,False);x=np.ones(seed.n);before=seed.solve(x)
        with self.assertRaises(PhaseResourceStopped):seed_pattern(seed,LIB,123,max_work=1)
        np.testing.assert_array_equal(seed.solve(x),before);self.assertTrue(seed.input_unchanged());seed.close()
        with self.assertRaises(ValueError):seed_pattern(seed,LIB,0)
        with self.assertRaises(ValueError):SeedInformedBlockIC0(t,LIB,True,pattern_reservation_bytes=0)
    def test_current_prefix_and_previous_native_prefix(self):
        self.assertTrue((R / 'scripts/cfd_reference3d_seed_fill1.c').read_bytes().startswith((R / 'scripts/cfd_reference3d_bounded_fill1.c').read_bytes()))
if __name__=='__main__':unittest.main()
