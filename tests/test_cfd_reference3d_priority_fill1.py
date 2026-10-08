import ctypes as ct
import gc
import json
import sys
import unittest
import weakref
from pathlib import Path
from unittest.mock import patch
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
LIB=library_path('build/c3d-priority-fill1/support/factor.dylib')
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_priority_fill1_block import BlockIC0,pattern

def fixture(equal=False):
    n=15;H=np.eye(n)*8
    for i in range(n):
        for j in ((i+1)%n,(i+4)%n):
            H[i,j]=H[j,i]=-.2 if equal else -.05-.02*((i*17+j*13)%29)
    U=np.eye(3) if equal else np.array([[1.,.08,.02],[.08,1.3,.1],[.02,.1,.7]])
    A=np.kron(H,U);p=np.arange(3*n).reshape(n,3).T.ravel();A=A[np.ix_(p,p)]
    return VectorTriangle(csr_matrix(np.triu(A)),LIB)
def oracle(t,perm):
    n=t.nodes;inverse=np.argsort(perm);physical=[{} for _ in range(n)];blocks=t.values.astype(np.float32).reshape(-1,3,3).astype(float)
    D=np.array([np.linalg.norm(blocks[t.starts[i]]) for i in range(n)])
    for i in range(n):
        for k in range(t.starts[i],t.starts[i+1]):
            j=t.rows[k];a,b=sorted((int(inverse[i]),int(inverse[j])));physical[a][b]=np.linalg.norm(blocks[k])/np.sqrt(D[i])/np.sqrt(D[j])
    paths=[{} for _ in range(n)]
    for col in range(n):
        neighbors=sorted(set(physical[col])-{col})
        for q,i in enumerate(neighbors):
            for j in neighbors[q+1:]:paths[i].setdefault(j,[]).append(physical[col][i]*physical[col][j])
    rows=[];starts=[0];complete=0;kept=0
    for i in range(n):
        extras={j:sum(sorted(v,reverse=True)) for j,v in paths[i].items() if j not in physical[i]};quota=len(physical[i])-1
        picked=sorted(extras,key=lambda j:(-extras[j],j))[:quota];complete+=len(extras);kept+=len(picked)
        rows.extend(sorted(set(physical[i])|set(picked)));starts.append(len(rows))
    return np.array(starts),np.array(rows),complete,kept
class Priority(unittest.TestCase):
    def test_independent_weighted_graph_oracle_duplicates_ties_and_quotas(self):
        from cfd_reference3d_bounded_fill1 import pattern as old_pattern
        changed=False
        for equal in (False,True):
            t=fixture(equal);st,rr,perm,inv,meta=pattern(t,LIB);a,b,complete,kept=oracle(t,perm)
            np.testing.assert_array_equal(st,a);np.testing.assert_array_equal(rr,b)
            self.assertEqual(meta['complete_level_one_fill'],complete);self.assertEqual(meta['kept_fill'],kept)
            self.assertLessEqual(len(rr),2*len(t.rows)-t.nodes);self.assertTrue(t.input_unchanged())
            old=old_pattern(t,LIB);np.testing.assert_array_equal(perm,old[2]);np.testing.assert_array_equal(st,old[0]);changed|=not np.array_equal(rr,old[1])
        self.assertTrue(changed)
    def test_reconstructed_factor_inverse_positive_error_model_and_storage(self):
        from test_cfd_reference3d_bounded_fill1 import dense_lower
        from cfd_reference3d_bounded_fill1 import BlockIC0 as Old
        t=fixture();f=BlockIC0(t,LIB,False);old=Old(t,LIB,False);L,p=dense_lower(f);model=(L@L.T)[np.ix_(p,p)];x=np.random.default_rng(1821).normal(size=f.n)
        np.testing.assert_allclose(f.solve(x),np.linalg.solve(model,x),rtol=1e-10,atol=1e-10)
        rounded=np.zeros_like(model)
        for j in range(t.nodes):
            for k in range(t.starts[j],t.starts[j+1]):
                i=t.rows[k];U=t.values.astype(np.float32).reshape(-1,3,3)[k].astype(float);ix=np.arange(3)*t.nodes+j;iy=np.arange(3)*t.nodes+i;rounded[np.ix_(ix,iy)]=U;rounded[np.ix_(iy,ix)]=U.T
        self.assertGreater(np.linalg.eigvalsh(model).min(),0);self.assertGreaterEqual(np.linalg.eigvalsh(model-rounded).min(),-1e-10)
        self.assertEqual(f.metadata['shifted_pivots'],0);self.assertEqual(f.metadata['factor_storage_bytes'],old.metadata['factor_storage_bytes']);self.assertEqual(f.metadata['rounded_values_sha256'],old.metadata['rounded_values_sha256'])
        self.assertTrue(f.input_unchanged() and old.input_unchanged());f.close();old.close()
    def test_actual_anisotropic_FE_pressure_original_inputs_and_owners(self):
        from test_cfd_reference3d_distributed_p3_cg8 import fixture as actual
        from cfd_reference3d_priority_fill1_pressure import PriorityFill1PressureFactor
        _,s=actual();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LIB);f=PriorityFill1PressureFactor(t,LIB,pressure_control=False,coarse_library=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib'),Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
        x,y=np.random.default_rng(1822).normal(size=(2,nv));px,py=f.pressure_solve(x),f.pressure_solve(y)
        self.assertGreater(x@px,0);self.assertGreater(x@f.solve(x),0);np.testing.assert_allclose(x@py,y@px,rtol=1e-10,atol=1e-7)
        self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['local_inner_iteration_cap'],8)
        refs=[weakref.ref(f.starts),weakref.ref(f.linear_pressure),weakref.ref(f.balanced),weakref.ref(f.coarse_factor.starts)];f.close();del f;gc.collect();self.assertTrue(all(r() is None for r in refs))
    def test_invalid_pattern_preflight_and_complete_symbolic_reservation(self):
        from cfd_reference3d_domain_budget import PhaseResourceStopped
        t=fixture()
        with self.assertRaises(ValueError):pattern(t,LIB,max_work=1)
        with self.assertRaises(ValueError):BlockIC0(t,LIB,True,pattern_reservation_bytes=0)
        with patch('cfd_reference3d_priority_fill1_block.current_rss_bytes',return_value=1800*2**20-32*2**20):
            with self.assertRaises(PhaseResourceStopped) as cm:BlockIC0(t,LIB,True,pattern_reservation_bytes=123456)
        a=cm.exception.record;self.assertEqual(a['complete_work_reservation_bytes'],123456);self.assertEqual(a['estimated_stage_bytes'],1800*2**20+123456+a['construction_workspace_bound_bytes'])
        lib=ct.CDLL(str(LIB));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);fp=ct.POINTER(ct.c_float);create=lib.cfd_priority_fill1_pattern_create;create.argtypes=(ct.c_int,lp,ip,ip,fp,ct.c_size_t,ip);create.restype=ct.c_void_p
        inv=np.arange(t.nodes,dtype=np.int32);v=t.values.astype(np.float32);v[0]=np.nan;status=ct.c_int();p=create(t.nodes,t.starts.ctypes.data_as(lp),t.rows.ctypes.data_as(ip),inv.ctypes.data_as(ip),v.ctypes.data_as(fp),384*2**20,ct.byref(status));self.assertFalse(p);self.assertEqual(status.value,-411);self.assertTrue(t.input_unchanged())
    def test_current_contracts_prefix_and_storage_bound(self):
        self.assertTrue((R / 'scripts/cfd_reference3d_priority_fill1.c').read_bytes().startswith((R / 'scripts/cfd_reference3d_bounded_fill1.c').read_bytes()))
        t = fixture()
        _, _, _, _, m = pattern(t, LIB)
        self.assertGreaterEqual(m['construction_workspace_bound_bytes'], 16 * m['candidate_pairs'] + 40 * m['original_blocks'] + 44 * (t.nodes + 1))
if __name__=='__main__':unittest.main()
