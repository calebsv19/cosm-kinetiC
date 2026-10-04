"""Full original FE action, symmetric storage and exact prefix inverse proofs."""
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix,triu
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from test_cfd_reference3d_bounded_condensed import fixture
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_bounded_condensed import BoundedCondensedSystem
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_quartic_pair import assemble_quartic
LIB=ROOT/'build/c3d-cholesky/support/factor.dylib'


class SharedFactor(unittest.TestCase):
    def test_original_action_anisotropic_refined_and_batches(self):
        for refined in (False,True):
            mesh=fixture(refined);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
            for batch in (1,7,64):
                new=TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=batch)
                C=BlockTriangle(new.upper_matrix,new.upper_matrix.shape[0]-new.nmacro);expected=old.matrix[new.retained_free][:,new.retained_free]
                rng=np.random.default_rng(671);x=rng.normal(size=(C.shape[0],3))
                np.testing.assert_allclose(C@x,expected@x,rtol=1e-10,atol=1e-10)
                np.testing.assert_allclose(C@x[:,0],expected@x[:,0],rtol=1e-10,atol=1e-10)
                np.testing.assert_allclose(C@x[:,0,None],expected@x[:,0,None],rtol=1e-10,atol=1e-10)
                np.testing.assert_allclose(C.rmatvec(x[:,0]),C@x[:,0],atol=0,rtol=0)
                self.assertAlmostEqual(float(x[:,0]@(C@x[:,1])),float(x[:,1]@(C@x[:,0])),places=10)
                self.assertEqual(new.metadata['assembly']['coo_batch_allocation_bytes'],batch*103*104//2*16)
                delta=new.upper_matrix-triu(expected,format='csr')
                self.assertLess(np.linalg.norm(delta.data)/np.linalg.norm(expected.data),1e-12)

    def test_pressure_diagonal_is_bitwise_preserved(self):
        mesh=fixture(True);old=BoundedCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        new=TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        pressure=old.reduced_matrix.diagonal()[-old.nmacro:]
        self.assertTrue(np.any(pressure!=0))
        C=BlockTriangle(new.upper_matrix,new.upper_matrix.shape[0]-new.nmacro)
        np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        np.testing.assert_array_equal(new.upper_matrix.diagonal()[-old.nmacro:],pressure)
        np.testing.assert_array_equal(new.retained_free,old.retained_free)

    def test_nonzero_local_rhs_reconstruction_and_independent_full_equations(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        new=TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        rng=np.random.default_rng(672);rhs=rng.normal(size=3*old.ub.N+old.pb.N)*.01
        z=np.zeros(old.shape[0]);z[new.retained_free]=rng.normal(size=len(new.retained_free))
        np.testing.assert_allclose(new.reduce_rhs(rhs),old.reduce_rhs(rhs),rtol=0,atol=1e-12)
        u,p=new.reconstruct(z,rhs);old_u,old_p=old.reconstruct(z,rhs)
        np.testing.assert_allclose(u,old_u,rtol=0,atol=1e-12);np.testing.assert_allclose(p,old_p,rtol=0,atol=1e-12)
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        actual=full_action(mesh,ub,pb,u,p,.1,128)
        explicit=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,explicit,rtol=1e-10,atol=1e-9)
        trace=(actual-rhs)[:3*ub.N].reshape(3,-1)[:,new.trace].ravel()
        pressure=(actual-rhs)[3*ub.N:]
        reduced_pressure=np.array([pressure[ids].sum() for _,_,ids,_ in new.records])
        expected=BlockTriangle(new.upper_matrix,new.upper_matrix.shape[0]-new.nmacro)@z[new.retained_free]-new.reduce_rhs(rhs)[new.retained_free]
        np.testing.assert_allclose(np.r_[trace,reduced_pressure][new.retained_free],expected,rtol=1e-10,atol=1e-9)

    def test_factor_matches_dense_inverse_both_orders_and_preserves_inputs(self):
        rng=np.random.default_rng(713);a=rng.normal(size=(37,37));v=a.T@a+np.eye(37)*.01
        mixed=np.zeros((40,40));mixed[:37,:37]=v;mixed[37:,37:]=[[-1.,.2,.1],[.2,-2.,.4],[.1,.4,-3.]]
        mixed[:37,37:]=rng.normal(size=(37,3));mixed[37:,:37]=mixed[:37,37:].T
        C=BlockTriangle(triu(csr_matrix(mixed),format='csr'),37)
        xfull=rng.normal(size=40);np.testing.assert_allclose(C@xfull,mixed@xfull,rtol=1e-12,atol=1e-12)
        upper=C.velocity.upper;saved=(upper.data.copy(),upper.indices.copy(),upper.indptr.copy())
        x=rng.normal(size=37);y=rng.normal(size=37);original=x.copy()
        for order in ('amd','metis'):
            factor=SharedTriangleFactor(upper,LIB,order)
            self.assertTrue(np.shares_memory(factor.rows,upper.indices))
            self.assertTrue(np.shares_memory(factor.values,upper.data))
            self.assertFalse(np.shares_memory(factor.starts,upper.indptr))
            self.assertEqual(factor.metadata['factor_input_allocation_bytes'],(37+1)*8)
            np.testing.assert_allclose(factor.solve(x),np.linalg.solve(v,x),atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(factor.solve(x+2*y),factor.solve(x)+2*factor.solve(y),atol=1e-10,rtol=1e-10)
            self.assertTrue(factor.input_unchanged())
            np.testing.assert_array_equal(factor.starts,upper.indptr)
            self.assertGreater(x@factor.solve(x),0)
            self.assertAlmostEqual(float(x@factor.solve(y)),float(y@factor.solve(x)),places=9)
            np.testing.assert_array_equal(x,original)
            for got,want in zip((upper.data,upper.indices,upper.indptr),saved):np.testing.assert_array_equal(got,want)
            factor.close();factor.close()
            with self.assertRaises(ValueError):factor.solve(x)

    def test_factor_anisotropic_fe_velocity_prefix(self):
        system=TriangleCondensedSystem(fixture(True),.1,fixed_boundaries=('walls',),assembly_batch=7)
        n=system.upper_matrix.shape[0]-system.nmacro
        upper=system.upper_matrix;dense=upper[:n,:n].toarray();dense=dense+dense.T-np.diag(np.diag(dense))
        x=np.random.default_rng(714).normal(size=n)
        C=BlockTriangle(upper,n)
        factor=SharedTriangleFactor(C.velocity.upper,LIB,'metis')
        np.testing.assert_allclose(dense@factor.solve(x),x,atol=1e-9,rtol=1e-9);factor.close()

    def test_invalid_structure_and_nonpositive_prefix_rejected(self):
        invalid=[csr_matrix([[1.,0],[.1,1.]]),csr_matrix([[1.,np.nan],[0.,1.]]),csr_matrix((0,0)),np.eye(2)]
        unsorted=csr_matrix(([.1,1.,1.],([0,0,1],[1,0,1])),shape=(2,2))
        unsorted.indices[:2]=[1,0];unsorted.data[:2]=[.1,1.];unsorted.has_sorted_indices=False
        invalid.append(unsorted)
        for matrix in invalid:
            with self.assertRaises(ValueError):BlockTriangle(matrix,1)
            with self.assertRaises(ValueError):SharedTriangleFactor(matrix,LIB)
        for matrix in (csr_matrix([[1.,0],[0.,-1.]]),csr_matrix([[1.,1],[0.,1.]])):
            with self.assertRaises(ValueError):SharedTriangleFactor(matrix,LIB)
        good=csr_matrix(np.eye(2))
        for prefix in (0,2,3,1.5):
            with self.assertRaises(ValueError):BlockTriangle(good,prefix)
        factor=SharedTriangleFactor(good,LIB)
        for rhs in (np.ones(3),np.array([np.nan,1.])):
            with self.assertRaises(ValueError):factor.solve(rhs)
        factor.close()
        for batch in (0,257,1.5):
            with self.assertRaises(ValueError):TriangleCondensedSystem(fixture(),.1,assembly_batch=batch)

    def test_shared_owner_lifetime_and_invalid_c_arrays(self):
        import weakref,gc
        upper=csr_matrix([[2.,.1],[0.,1.]])
        ref=weakref.ref(upper);factor=SharedTriangleFactor(upper,LIB)
        before=(upper.data.copy(),upper.indices.copy(),upper.indptr.copy())
        del upper;gc.collect();self.assertIsNotNone(ref())
        np.testing.assert_allclose(factor.solve(np.ones(2)),np.linalg.solve([[2.,.1],[.1,1.]],np.ones(2)))
        for got,want in zip((ref().data,ref().indices,ref().indptr),before):np.testing.assert_array_equal(got,want)
        factor.close();del factor;gc.collect();self.assertIsNone(ref())
        invalid=csr_matrix([[2.,.1],[0.,1.]]);invalid.indices[1]=2
        with self.assertRaises(ValueError):SharedTriangleFactor(invalid,LIB)
        invalid=csr_matrix([[2.,.1],[0.,1.]],dtype=np.float32)
        with self.assertRaises(ValueError):SharedTriangleFactor(invalid,LIB)
        invalid=csr_matrix([[2.,.1],[0.,1.]]);invalid.indptr[-1]-=1
        with self.assertRaises(ValueError):SharedTriangleFactor(invalid,LIB)

    def test_repeated_owned_factor_lifecycle(self):
        for _ in range(100):
            factor=SharedTriangleFactor(csr_matrix([[2.,.1],[0.,1.]]),LIB)
            np.testing.assert_allclose(factor.solve(np.ones(2)),np.linalg.solve([[2.,.1],[.1,1.]],np.ones(2)))
            factor.close()


if __name__=='__main__':unittest.main()
