"""Exact scalar permutation, complete block graph and owned symbolic cost proofs."""
import sys
import unittest
import gc
import weakref
import io,json
from contextlib import redirect_stdout
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from test_cfd_reference3d_bounded_condensed import fixture
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_symbolic import velocity_graph,SymbolicFactor
LIB=ROOT/'build/c3d-symbolic/support/symbolic.dylib'
NUMERIC=ROOT/'build/c3d-cholesky/support/factor.dylib'


def velocity_fixture(refined):
    system=TriangleCondensedSystem(fixture(refined),.1,fixed_boundaries=('walls',),assembly_batch=7)
    C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
    return C.velocity.upper,C.nv//3


class Symbolic(unittest.TestCase):
    def test_scalar_bijection_coefficients_and_physical_action(self):
        for refined in (False,True):
            upper,nodes=velocity_fixture(refined)
            new,permutation,block=velocity_graph(upper,nodes,'interleaved_scalar')
            self.assertEqual(block,1)
            np.testing.assert_array_equal(np.sort(permutation),np.arange(3*nodes))
            np.testing.assert_array_equal(np.sort(new.data),np.sort(upper.data))
            np.testing.assert_array_equal(new.diagonal(),upper.diagonal()[permutation])
            x=np.random.default_rng(861).normal(size=(3*nodes,3))
            np.testing.assert_allclose(SymmetricTriangle(new)@x[permutation],(SymmetricTriangle(upper)@x)[permutation],rtol=1e-11,atol=1e-10)

    def test_complete_block_graph_covers_every_component_pair(self):
        upper,nodes=velocity_fixture(True);graph,permutation,block=velocity_graph(upper,nodes,'vector_block')
        self.assertIsNone(permutation);self.assertEqual(block,3)
        entries=upper.tocoo();expected={tuple(sorted((int(r%nodes),int(c%nodes)))) for r,c in zip(entries.row,entries.col)}
        actual=graph.tocoo();pairs=set(zip(actual.row,actual.col))
        self.assertEqual(pairs,expected)
        np.testing.assert_array_equal(graph.data,np.ones(len(expected)))
        self.assertTrue(all((i,i) in pairs for i in range(nodes)))

    def test_control_storage_matches_actual_exact_factor_and_preserves_inputs(self):
        upper,nodes=velocity_fixture(True)
        saved=(upper.data.copy(),upper.indices.copy(),upper.indptr.copy())
        for order in ('amd','metis'):
            symbolic=SymbolicFactor(upper,LIB,order)
            numeric=SharedTriangleFactor(upper,NUMERIC,order)
            self.assertEqual(symbolic.metadata['factor_storage_bytes'],numeric.metadata['symbolic_factor_storage_bytes'])
            self.assertGreater(symbolic.metadata['numeric_workspace_bytes'],0)
            self.assertTrue(symbolic.input_unchanged());self.assertTrue(numeric.input_unchanged())
            for a,b in zip((upper.data,upper.indices,upper.indptr),saved):np.testing.assert_array_equal(a,b)
            self.assertEqual(symbolic.metadata['represented_scalar_dofs'],3*nodes)
            symbolic.close();numeric.close()

    def test_symbolic_is_not_a_numeric_inverse_or_spd_proof(self):
        positive=csr_matrix(np.diag([1.,2.,3.]));negative=csr_matrix(np.diag([1.,-2.,3.]))
        a=SymbolicFactor(positive,LIB);b=SymbolicFactor(negative,LIB)
        self.assertEqual(a.metadata['factor_storage_bytes'],b.metadata['factor_storage_bytes'])
        self.assertFalse(hasattr(a,'solve'));self.assertFalse(hasattr(b,'solve'))
        with self.assertRaises(ValueError):SharedTriangleFactor(negative,NUMERIC)
        a.close();b.close()

    def test_invalid_graph_or_options_rejected(self):
        for graph in (csr_matrix([[1.,0],[.1,1.]]),csr_matrix([[np.nan]]),csr_matrix((0,0)),csr_matrix(np.eye(2),dtype=np.float32)):
            with self.assertRaises(ValueError):SymbolicFactor(graph,LIB)
        good=csr_matrix(np.eye(3))
        for mode in ('bad',None):
            with self.assertRaises(ValueError):velocity_graph(good,1,mode)
        for nodes in (0,2,1.5):
            with self.assertRaises(ValueError):velocity_graph(good,nodes,'component_major')
        for block in (0,2,1.5,3.0):
            with self.assertRaises(ValueError):SymbolicFactor(good,LIB,block_size=block)
        with self.assertRaises(ValueError):SymbolicFactor(good,LIB,ordering='bad')
        bad=csr_matrix(np.eye(3));bad.indices[1]=3
        with self.assertRaises(ValueError):SymbolicFactor(bad,LIB)

    def test_complete_probe_result_serializes_numpy_dof_counts(self):
        from cfd_reference3d_symbolic_probe import run
        mesh=fixture(False).with_boundaries({'inlet':lambda x:np.isclose(x[0],0.),'body':lambda x:np.zeros(x.shape[1],dtype=bool)})
        small=(mesh,np.array([.5,.2,.05]),np.array([.8,.5,.15]),[np.unique(a) for a in mesh.p],mesh.nelements//4)
        with patch('cfd_reference3d_symbolic_probe.domain_mesh',return_value=small),redirect_stdout(io.StringIO()):
            row=run(symbolic_library=LIB)
        saved=json.loads(json.dumps(row))
        self.assertTrue(saved['diagnostic_accepted'])
        self.assertFalse(saved['numerically_accepted'] or saved['physical_accuracy_certified'] or saved['numerical_field_published'])
        self.assertIsInstance(saved['full_velocity_dofs'],int);self.assertIsInstance(saved['full_pressure_dofs'],int)

    def test_block_symbolic_lifetime_and_repeated_cleanup(self):
        graph=csr_matrix([[1.,1.],[0.,1.]])
        ref=weakref.ref(graph);factor=SymbolicFactor(graph,LIB,block_size=3)
        self.assertEqual(factor.metadata['represented_scalar_dofs'],6)
        del graph;gc.collect();self.assertIsNotNone(ref());self.assertTrue(factor.input_unchanged())
        factor.close();factor.close();del factor;gc.collect();self.assertIsNone(ref())
        for _ in range(100):
            factor=SymbolicFactor(csr_matrix([[1.,1.],[0.,1.]]),LIB,block_size=3)
            self.assertTrue(factor.input_unchanged());factor.close()


if __name__=='__main__':unittest.main()
