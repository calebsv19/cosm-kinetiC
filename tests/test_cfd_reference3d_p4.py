"""Quartic nodal/conformity support and cubic divergence/mass controls."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,InteriorFacetBasis,BilinearForm,ElementDG,asm
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split,ElementTetP3
from cfd_reference3d_p4 import ElementTetP4
from cfd_reference3d_quartic_pair import assemble_quartic,CubicPressureMass,quartic_quadrature
from cfd_reference3d_chunked import cell_basis


class QuarticSupport(unittest.TestCase):
    def test_nodal_partition_and_gradients(self):
        element=ElementTetP4();X=element.doflocs.T
        matrix=np.array([element.lbasis(X,i)[0] for i in range(35)])
        np.testing.assert_allclose(matrix,np.eye(35),atol=1e-13)
        points=np.random.default_rng(90).dirichlet(np.ones(4),size=31).T[1:]
        values=[element.lbasis(points,i) for i in range(35)]
        np.testing.assert_allclose(sum(v[0] for v in values),1.,atol=1e-13)
        np.testing.assert_allclose(sum(v[1] for v in values),0.,atol=1e-13)
        h=1e-6
        for axis in range(3):
            delta=np.zeros_like(points);delta[axis]=h
            for i in (0,7,21,29,34):
                fd=(element.lbasis(points+delta,i)[0]-element.lbasis(points-delta,i)[0])/(2*h)
                np.testing.assert_allclose(fd,element.lbasis(points,i)[1][axis],rtol=1e-7,atol=1e-8)

    def test_quartic_reproduction_and_random_shared_traces(self):
        parent=MeshTet.init_tensor(np.linspace(0,1,3),np.linspace(0,1.5,3),np.linspace(0,2,3))
        mesh=alfeld_split(parent);ub,pb,*_=assemble_quartic(mesh,.1,64)
        all_basis=cell_basis(ub,np.arange(mesh.nelements));x,y,z=ub.doflocs
        coefficients=x**4+2*x*x*y*y+z**4+x*y*z*z
        X,Y,Z=all_basis.global_coordinates();field=all_basis.interpolate(coefficients)
        np.testing.assert_allclose(field,X**4+2*X*X*Y*Y+Z**4+X*Y*Z*Z,atol=1e-11)
        gradient=np.array([4*X**3+4*X*Y*Y+Y*Z*Z,4*X*X*Y+X*Z*Z,4*Z**3+2*X*Y*Z])
        np.testing.assert_allclose(field.grad,gradient,atol=1e-10)
        facets=np.flatnonzero(mesh.f2t[1]!=-1)
        left=InteriorFacetBasis(mesh,ub.elem,facets=facets,side=0,intorder=6,dofs=ub.dofs)
        right=InteriorFacetBasis(mesh,ub.elem,facets=facets,side=1,quadrature=left.quadrature,dofs=ub.dofs)
        random=np.random.default_rng(91).normal(size=ub.N)
        np.testing.assert_allclose(left.interpolate(random),right.interpolate(random),atol=1e-11)

    def test_divergence_span_and_mass_scaling(self):
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.])))
        ub,pb,A,B=assemble_quartic(mesh,.1,7);mass=CubicPressureMass(pb,.1)
        rng=np.random.default_rng(92);u=rng.normal(size=(3,ub.N));p=rng.normal(size=pb.N)
        fullu=cell_basis(ub,np.arange(mesh.nelements));fullp=cell_basis(pb,np.arange(mesh.nelements))
        @BilinearForm
        def inner(v,q,w):return v*q
        M=asm(inner,fullp)
        np.testing.assert_allclose(mass.apply(p),M@p,rtol=1e-12,atol=1e-13)
        np.testing.assert_allclose(mass.solve(M@p),p,rtol=1e-11,atol=1e-11)
        np.testing.assert_allclose(CubicPressureMass(pb,.2).precondition(p),2*mass.precondition(p),rtol=1e-12)
        divergence=mass.solve(-sum(B[a]@u[a] for a in range(3)))
        exact=sum(fullu.interpolate(u[a]).grad[a] for a in range(3))
        np.testing.assert_allclose(fullp.interpolate(divergence),exact,rtol=1e-11,atol=1e-10)

    def test_degree_six_quadrature_and_underintegration_guard(self):
        import math
        X,W=quartic_quadrature()
        for i in range(7):
            for j in range(7-i):
                for k in range(7-i-j):
                    exact=math.factorial(i)*math.factorial(j)*math.factorial(k)/math.factorial(i+j+k+3)
                    self.assertAlmostEqual(float(np.sum(X[0]**i*X[1]**j*X[2]**k*W)),exact,places=13)
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.]),np.array([0.,1.])))
        bad=Basis(mesh,ElementDG(ElementTetP3()),intorder=6,elements=np.array([0]))
        with self.assertRaises(ValueError):CubicPressureMass(bad,.1)

    def test_all_local_mean_zero_pressure_modes(self):
        parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.]))
        mesh=alfeld_split(parent);ub,pb,A,B=assemble_quartic(mesh,.1,64)
        for macro in range(parent.nelements):
            cells=macro+parent.nelements*np.arange(4)
            faces,counts=np.unique(mesh.t2f[:,cells],return_counts=True)
            boundary=ub.get_dofs(facets=faces[counts==1]).all()
            interior=np.setdiff1d(np.unique(ub.dofs.element_dofs[:,cells]),boundary)
            rows=pb.dofs.element_dofs[:,cells].T.ravel()
            coupling=np.hstack([b[rows][:,interior].toarray() for b in B])
            self.assertEqual(coupling.shape,(80,105))
            singular=np.linalg.svd(coupling,compute_uv=False)
            self.assertEqual(np.count_nonzero(singular>singular[0]*1e-10),79)
            self.assertLess(np.linalg.norm(coupling.T@np.ones(80))/np.linalg.norm(coupling),1e-12)

    def test_degree_seven_boundary_quadrature(self):
        import math
        from skfem.quadrature import get_quadrature_tri
        X,W=get_quadrature_tri(8)
        for i in range(8):
            for j in range(8-i):
                exact=math.factorial(i)*math.factorial(j)/math.factorial(i+j+2)
                self.assertAlmostEqual(float(np.sum(X[0]**i*X[1]**j*W)),exact,places=13)

    def test_unsorted_orientation_rejected(self):
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.]),np.array([0.,1.])))
        bad=mesh.t.copy();bad[[0,1]]=bad[[1,0]]
        with self.assertRaises(ValueError):assemble_quartic(MeshTet(mesh.p,bad),.1)


if __name__=='__main__':unittest.main()
