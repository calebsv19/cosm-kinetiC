#!/usr/bin/env python3
"""Same continuous P2/P1 Stokes reference with scalar storage and graded meshes."""
import argparse
import json
import resource
import time
from pathlib import Path
import numpy as np
from scipy.sparse import bmat, hstack, kron, eye
from scipy.sparse.linalg import LinearOperator, minres
from skfem import (MeshTet, ElementTetP2, ElementTetP1, Basis, FacetBasis,
                   Functional, LinearForm, BilinearForm, asm)
from skfem.models.poisson import laplace, mass
from pyamg import smoothed_aggregation_solver
import skfem, scipy, pyamg
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_traction import traction


def graded_axis(length, lo, hi, body_count, gap_count, wall=False):
    t = np.linspace(0, 1, gap_count+1)
    body = lo + (hi-lo)*.5*(1-np.cos(np.pi*np.linspace(0, 1, body_count+1)))
    left = lo-lo*t[::-1]**3
    right = hi+(length-hi)*t**3
    return np.r_[left[:-1], body, right[1:]]


def reference(n=8, L=8., cx=4., levels=0, body=True, mu=.1, Q=.008,
              graded=0, gap_count=4, long_count=6, grad_div=0., mirror=False, edge_passes=0, mesh_only=False, pressure_output=None, edge_radii=None, body_counts=None, physical_long=False, split_normal=False, velocity_output=None, consistency=False):
    started = time.monotonic()
    assert L in (4, 6, 8) and n % 4 == 0 and .5 < cx < L-.5
    lo, hi = np.array([cx-.5,.5,.5]), np.array([cx+.5,1.5,1.5])
    counts = [graded]*3 if body_counts is None else body_counts
    if graded:
        assert len(counts)==3 and all(c>0 and c%2==0 for c in counts)
        axes = [graded_axis(L,lo[0],hi[0],counts[0],long_count)]
        axes += [graded_axis(2,lo[a],hi[a],counts[a],gap_count,True) for a in (1,2)]
        if physical_long:
            assert cx == L/2
            d=3.5*(np.arange(7)/6)**3
            d=np.r_[d[d<lo[0]-1e-12],lo[0]]
            if split_normal: d=np.sort(np.r_[d,d[1]/2])
            interior=axes[0][(axes[0]>=lo[0]-1e-12)&(axes[0]<=hi[0]+1e-12)]
            axes[0]=np.r_[lo[0]-d[:0:-1],interior,hi[0]+d[1:]]
    else:
        axes = [np.linspace(0,L,round(L*n/2)+1),np.linspace(0,2,n+1),np.linspace(0,2,n+1)]
    edge_counts = []
    if edge_passes:
        assert mirror and body and cx == L/2
        mesh,edge_counts = refined_octant(axes,np.array([L,2,2]),lo,hi,edge_passes,radii=edge_radii)
    elif mirror:
        assert graded and graded % 2 == 0
        lengths = np.array([L,2,2])
        octant = MeshTet.init_tensor(*[x[x<=lengths[a]/2+1e-12] for a,x in enumerate(axes)])
        points, elements = [], []
        for side in range(8):
            coordinates = octant.p.copy()
            for a in range(3):
                if side & (1<<a): coordinates[a] = lengths[a]-coordinates[a]
            elements.append(octant.t+len(points)*octant.p.shape[1]);points.append(coordinates)
        combined = np.concatenate(points,axis=1)
        unique, inverse = np.unique(np.round(combined.T,12),axis=0,return_inverse=True)
        mesh = MeshTet(unique.T,inverse[np.concatenate(elements,axis=1)])
    else:
        mesh = MeshTet.init_tensor(*axes)
    if body and not edge_passes:
        c = mesh.p[:,mesh.t].mean(axis=1)
        mesh = mesh.remove_elements(np.nonzero(np.all((c>lo[:,None])&(c<hi[:,None]),axis=0))[0])
        for level in range(levels):
            c = mesh.p[:,mesh.t].mean(axis=1)
            distances = []
            for axis in range(3):
                others = [a for a in range(3) if a!=axis]
                for sa in (0,1):
                    for sb in (0,1):
                        d = np.maximum(np.maximum(lo[axis]-c[axis],c[axis]-hi[axis]),0)**2
                        d += (c[others[0]]-(hi if sa else lo)[others[0]])**2
                        d += (c[others[1]]-(hi if sb else lo)[others[1]])**2
                        distances.append(np.sqrt(d))
            mesh = mesh.refined(np.nonzero(np.min(distances,axis=0)<2/n/2**(level/3))[0])
    assert mesh.nelements <= 50000, ('reference_mesh_cap',mesh.nelements)
    if mesh_only:
        return dict(tetrahedra=int(mesh.nelements),edge_passes=edge_passes,edge_counts=edge_counts,mesh_cap=50000)
    def body_surface(x):
        within = np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)
        return within & np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh = mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),
            'outlet':lambda x:np.isclose(x[0],L),
            'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),
            'body':body_surface})
    ub, pb = Basis(mesh,ElementTetP2(),intorder=2), Basis(mesh,ElementTetP1(),intorder=2)
    A = mu*asm(laplace,ub)
    blocks = []
    for a in range(3):
        @BilinearForm
        def divergence(u,v,w):
            return -u.grad[a]*v
        blocks.append(asm(divergence,ub,pb))
    @LinearForm
    def inlet(v,w): return v
    f = asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=4))
    fixed = ub.get_dofs(['walls']+(['body'] if body else [])).all()
    free = np.setdiff1d(np.arange(ub.N),fixed)
    Af = A[free][:,free]
    Bf = hstack([b[:,free] for b in blocks],format='csr')
    stabilization = None
    if grad_div:
        stabilization = []
        for a in range(3):
            row = []
            for b in range(3):
                @BilinearForm
                def penalty(u,v,w): return u.grad[b]*v.grad[a]
                row.append(grad_div*asm(penalty,ub))
            stabilization.append(row)
        H = bmat([[((A if a==b else 0)+stabilization[a][b])[free][:,free]
                   for b in range(3)] for a in range(3)],format='csr')
        diagonal_blocks = [(A+stabilization[a][a])[free][:,free] for a in range(3)]
    else:
        H = kron(eye(3),Af,format='csr')
        diagonal_blocks = [Af]
    K = bmat([[H,Bf.T],[Bf,None]],format='csr')
    rhs = np.r_[f[free],np.zeros(2*len(free)+pb.N)]
    pcs = [smoothed_aggregation_solver(matrix,max_coarse=100,symmetry='symmetric',
           presmoother=('gauss_seidel',{'sweep':'symmetric'}),
           postsmoother=('gauss_seidel',{'sweep':'symmetric'})).aspreconditioner()
           for matrix in diagonal_blocks]
    if len(pcs)==1: pcs *= 3
    diagonal = asm(mass,pb).diagonal()/(mu+grad_div)
    nv = 3*len(free)
    def precondition(x):
        return np.r_[np.concatenate([pcs[a]@v for a,v in enumerate(x[:nv].reshape(3,-1))]),x[nv:]/diagonal]
    iterations = [0]
    class TrueResidualConverged(Exception):
        def __init__(self,x):self.x=x.copy()
    def callback(x):
        iterations[0] += 1
        if iterations[0]%10==0 and np.linalg.norm(K@x-rhs)/np.linalg.norm(rhs)<=1e-10:
            raise TrueResidualConverged(x)
    try:
        z, info = minres(K,rhs,M=LinearOperator(K.shape,precondition),rtol=1e-14,
                         maxiter=3000,callback=callback)
    except TrueResidualConverged as accepted:
        z,info=accepted.x,0
    residual_norm = np.linalg.norm(K@z-rhs)/np.linalg.norm(rhs)
    assert info==0 and residual_norm<1e-8,(info,residual_norm)
    u = np.zeros((3,ub.N));u[:,free] = z[:nv].reshape(3,-1);p = z[nv:]
    def facet(name): return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=4)
    fi, fo = facet('inlet'), facet('outlet')
    @Functional
    def flux(w): return np.einsum('i...,i...->...',w.velocity,w.n)
    def values(basis): return np.stack([basis.interpolate(v) for v in u])
    def gradients(basis): return np.stack([basis.interpolate(v).grad for v in u])
    qout = float(asm(flux,fo,velocity=values(fo)))
    Pin = Q/qout;u *= Pin;p *= Pin
    reactions = [A@u[a]+blocks[a].T@p-(f*Pin if a==0 else 0) for a in range(3)]
    unstabilized = [r.copy() for r in reactions]
    if stabilization is not None:
        for a in range(3):
            reactions[a] += sum(stabilization[a][b]@u[b] for b in range(3))
    pressure_force, viscous_force, raw_force, reaction = [0]*3,[0]*3,[0]*3,[0]*3
    if body:
        fb = facet('body');fp = FacetBasis(mesh,pb.elem,facets=mesh.boundaries['body'],intorder=4)
        g, pv = gradients(fb), fp.interpolate(p)
        for a in range(3):
            @Functional
            def pressure_traction(w): return w.pressure*w.n[a]
            @Functional
            def symmetric_traction(w):
                return -mu*np.einsum('j...,j...->...',w.gradient[a]+w.gradient[:,a],w.n)
            @Functional
            def trace_traction(w):
                return -mu*np.einsum('j...,j...->...',w.gradient[a],w.n)*(1-w.n[a]**2)
            pressure_force[a] = float(asm(pressure_traction,fb,pressure=pv))
            raw_force[a] = float(asm(symmetric_traction,fb,gradient=g))
            viscous_force[a] = float(asm(trace_traction,fb,gradient=g))
            reaction[a] = float(-reactions[a][ub.get_dofs('body').all()].sum())
    traction_checks = []
    if body:
        traction_checks = [traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (2,4,8)]
        for t in traction_checks:
            assert np.max(np.abs(np.array(t['raw_viscous_force_n'])-raw_force))<1e-11
            assert np.max(np.abs(np.array(t['pressure_force_n'])-pressure_force))<1e-11
    if pressure_output is not None:
        assert not pressure_output.exists(), pressure_output
        np.savez_compressed(pressure_output,vertices_m=mesh.p,tetrahedra=mesh.t,
                            pressure_pa=p[pb.nodal_dofs[0]],lo=lo,hi=hi,length=L)
    consistency_diagnostics = None
    if consistency and body:
        from cfd_reference3d_consistency import diagnose
        consistency_diagnostics = diagnose(mesh,ub,pb,u,p,mu,lo,hi)
    if velocity_output is not None:
        assert not velocity_output.exists(), velocity_output
        np.savez_compressed(velocity_output,vertices_m=mesh.p,tetrahedra=mesh.t,
                            velocity_coefficients=u,velocity_doflocs_m=ub.doflocs,
                            pressure_coefficients=p,lo=lo,hi=hi,mu=mu,length=L)
    @Functional
    def dissipation(w):
        g = w.gradient;e = .5*(g+g.swapaxes(0,1))
        return 2*mu*np.einsum('ij...,ij...->...',e,e)
    @Functional
    def divergence_squared(w):
        return np.einsum('ii...->...',w.gradient)**2
    @Functional
    def extra_work(w):
        return mu*np.einsum('i...,ji...,j...->...',w.velocity,w.gradient,w.n)
    D = float(asm(dissipation,ub,gradient=gradients(ub)))
    divergence_l2 = float(asm(divergence_squared,ub,gradient=gradients(ub)))**.5
    power = Pin*Q+sum(float(asm(extra_work,b,velocity=values(b),gradient=gradients(b))) for b in (fi,fo))
    return dict(schema='physics_sim_c3d8_fem_reference_v2',n=n,length=L,center_x=cx,
                body=body,levels=levels,edge_passes=edge_passes,edge_counts=edge_counts,edge_radii_m=edge_radii,traction_checks=traction_checks,edge_only=not graded,reflection_symmetric_mesh=mirror,graded_body_intervals=graded,
                graded_gap_intervals=gap_count,graded_long_intervals=long_count,graded_body_counts=counts,physical_long_nodes=physical_long,split_first_normal=split_normal,
                axis_nodes_m=[x.tolist() for x in axes],mu=mu,flow_m3_s=Q,inlet_pressure_pa=Pin,
                pressure_force_n=pressure_force,viscous_force_n=viscous_force,
                raw_symmetric_viscous_force_n=raw_force,reaction_force_n=reaction,grad_div_coefficient_pa_s=grad_div,
                unstabilized_reaction_force_n=[float(-r[ub.get_dofs('body').all()].sum()) for r in unstabilized] if body else [0]*3,
                stabilization_reaction_n=[float(-(reactions[a]-unstabilized[a])[ub.get_dofs('body').all()].sum()) for a in range(3)] if body else [0]*3,
                physical_dissipation_w=D,physical_boundary_power_w=power,
                physical_energy_imbalance=abs(power-D)/D,divergence_l2_s_inv_m_3_2=divergence_l2,
                free_relative_residual=float(residual_norm),flux_error=abs(float(asm(flux,fi,velocity=values(fi)))+Q)/Q,
                tetrahedra=int(mesh.nelements),velocity_dofs=int(3*ub.N),pressure_dofs=int(pb.N),
                iterations=iterations[0],wall_s=time.monotonic()-started,
                peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                consistency_diagnostics=consistency_diagnostics,
                method='scalar P2 blocks, P1 pressure; same vector-Laplacian weak Stokes',
                versions=dict(skfem=skfem.__version__,scipy=scipy.__version__,numpy=np.__version__,pyamg=pyamg.__version__))


if __name__=='__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--velocity-output',type=Path);ap.add_argument('--consistency-diagnostics',action='store_true')
    ap.add_argument('--n',type=int,default=8);ap.add_argument('--length',type=float,default=8)
    ap.add_argument('--center',type=float,default=4);ap.add_argument('--levels',type=int,default=0)
    ap.add_argument('--split-first-normal',action='store_true')
    ap.add_argument('--physical-long-grid',action='store_true')
    ap.add_argument('--body-counts',type=lambda s:[int(x) for x in s.split(',')])
    ap.add_argument('--edge-radii',type=lambda s:[float(x) for x in s.split(',')])
    ap.add_argument('--edge-passes',type=int,default=0);ap.add_argument('--mesh-only',action='store_true');ap.add_argument('--pressure-output',type=Path)
    ap.add_argument('--mirror-mesh',action='store_true');ap.add_argument('--grad-div',type=float,default=0);ap.add_argument('--graded',type=int,default=0);ap.add_argument('--gaps',type=int,default=4)
    ap.add_argument('--long',type=int,default=6);ap.add_argument('--empty',action='store_true')
    ap.add_argument('--output',type=Path,required=True)
    a = ap.parse_args();assert not a.output.exists(),a.output
    row = reference(a.n,a.length,a.center,a.levels,not a.empty,graded=a.graded,gap_count=a.gaps,long_count=a.long,grad_div=a.grad_div,mirror=a.mirror_mesh,edge_passes=a.edge_passes,mesh_only=a.mesh_only,pressure_output=a.pressure_output,edge_radii=a.edge_radii,body_counts=a.body_counts,physical_long=a.physical_long_grid,split_normal=a.split_first_normal,velocity_output=a.velocity_output,consistency=a.consistency_diagnostics)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps(row),flush=True)
