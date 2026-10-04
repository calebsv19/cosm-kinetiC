#!/usr/bin/env python3
"""Experimental Alfeld P3/DG-P2 fixed-flow Stokes reference with raw wall stress."""
import argparse
import json
import resource
import time
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import LinearOperator, minres
from skfem import MeshTet, FacetBasis, LinearForm, asm
from pyamg import smoothed_aggregation_solver
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_stokes_pair import assemble_pair, mixed_matrix
from cfd_reference3d_traction import traction
from cfd_reference3d_consistency import diagnose


def mesh_for_case(length=4.,body=True,count=2,split=False,n=4):
    lo=np.array([length/2-.5,.5,.5]);hi=lo+1
    lengths=np.array([length,2.,2.])
    if body:
        def axis(L,a):
            interior=lo[a]+(hi[a]-lo[a])*.5*(1-np.cos(np.pi*np.linspace(0,1,count+1)))
            left=lo[a]-lo[a]*np.linspace(1,0,3)**3
            right=hi[a]+(L-hi[a])*np.linspace(0,1,3)**3
            if split and a==0:
                left=np.sort(np.r_[left,lo[a]-(lo[a]-left[-2])/2])
                right=np.sort(np.r_[right,hi[a]+(right[1]-hi[a])/2])
            return np.r_[left[:-1],interior,right[1:]]
        axes=[axis(L,a) for a,L in enumerate(lengths)]
        octant=MeshTet.init_tensor(*[x[x<=lengths[a]/2+1e-12] for a,x in enumerate(axes)])
        points=[];elements=[]
        for side in range(8):
            x=octant.p.copy()
            for a in range(3):
                if side & (1<<a):x[a]=lengths[a]-x[a]
            elements.append(octant.t+len(points)*octant.nvertices);points.append(x)
        unique,inverse=np.unique(np.round(np.concatenate(points,axis=1).T,12),axis=0,return_inverse=True)
        macro=MeshTet(unique.T,inverse[np.concatenate(elements,axis=1)])
        c=macro.p[:,macro.t].mean(axis=1)
        macro=macro.remove_elements(np.nonzero(np.all((c>lo[:,None])&(c<hi[:,None]),axis=0))[0])
    else:
        axes=[np.linspace(0,length,int(length*n/2)+1),np.linspace(0,2,n+1),np.linspace(0,2,n+1)]
        macro=MeshTet.init_tensor(*axes)
    mesh=alfeld_split(macro)
    if mesh.nelements>50000:raise ValueError(('reference_mesh_cap',mesh.nelements))
    def surface(x):
        return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0) & np.any(
            np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),
        'outlet':lambda x:np.isclose(x[0],length),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),
        'body':surface})
    # No artificial internal plane may become a wall.
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    exterior=np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)
    assert np.all(exterior|surface(mid)) if body else np.all(exterior)
    return mesh,lo,hi,axes,macro.nelements


def reference(length=4.,body=True,count=2,split=False,n=4,mesh_only=False,snapshot=None):
    started=time.monotonic();mu=.1;Q=.008
    mesh,lo,hi,axes,macro_count=mesh_for_case(length,body,count,split,n)
    dimensions=dict(tetrahedra=mesh.nelements,macro_tetrahedra=macro_count,
        velocity_dofs=int(3*(mesh.nvertices+2*mesh.nedges+mesh.nfacets)),pressure_dofs=int(10*mesh.nelements))
    print(json.dumps({'phase':'preassembly',**dimensions}),flush=True)
    if mesh_only:return dimensions
    timings={}
    ub,pb,A,blocks=assemble_pair(mesh,mu);timings['assembly_s']=time.monotonic()-started
    fixed=ub.get_dofs(['walls']+(['body'] if body else [])).all()
    free=np.setdiff1d(np.arange(ub.N),fixed);K=mixed_matrix(A,blocks,free)
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=6))
    rhs=np.r_[f[free],np.zeros(2*len(free)+pb.N)];rhs_norm=np.linalg.norm(rhs)
    pc=smoothed_aggregation_solver(A[free][:,free],max_coarse=100,symmetry='symmetric',
        presmoother=('gauss_seidel',{'sweep':'symmetric'}),
        postsmoother=('gauss_seidel',{'sweep':'symmetric'})).aspreconditioner()
    # Exact inverse of each element's DG pressure mass block, scaled by mu.
    # No stabilization or pressure smoothing is introduced into the equations.
    shape=np.array([entry[0][0] for entry in pb.basis])
    inverse=np.linalg.inv(np.einsum('iq,jq,q->ij',shape,shape,pb.W))
    determinant=pb.dx.sum(axis=1)/pb.W.sum();nv=3*len(free)
    def precondition(x):
        pressure=(mu*(inverse@x[nv:].reshape(-1,10).T)/determinant[None]).T.ravel()
        return np.r_[np.concatenate([pc@v for v in x[:nv].reshape(3,-1)]),pressure]
    timings['setup_s']=time.monotonic()-started-timings['assembly_s']
    print(json.dumps({'phase':'solve','matrix_nnz':K.nnz,**timings}),flush=True)
    iterations=[0]
    class Converged(Exception):
        def __init__(self,x):self.x=x.copy()
    def callback(x):
        iterations[0]+=1
        if iterations[0]%10==0:
            residual=np.linalg.norm(K@x-rhs)/rhs_norm
            if iterations[0]%100==0:print(json.dumps({'iteration':iterations[0],'true_residual':residual}),flush=True)
            if residual<=1e-10:raise Converged(x)
    begin=time.monotonic()
    try:
        result,info=minres(K,rhs,M=LinearOperator(K.shape,precondition),rtol=1e-14,maxiter=3000,callback=callback)
    except Converged as accepted:result,info=accepted.x,0
    residual=float(np.linalg.norm(K@result-rhs)/rhs_norm)
    if info!=0 or residual>=1e-8:raise ValueError(('linear_residual',info,residual))
    timings['solve_s']=time.monotonic()-begin
    u=np.zeros((3,ub.N));u[:,free]=result[:nv].reshape(3,-1);p=result[nv:]
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=6)
    def values(b):return np.stack([b.interpolate(v) for v in u])
    def gradients(b):return np.stack([b.interpolate(v).grad for v in u])
    fi,fo=facet('inlet'),facet('outlet')
    def flux(b):return float(np.sum(np.einsum('i...,i...->...',values(b),b.normals)*b.dx))
    response=flux(fo)
    if not np.isfinite(response) or response<=0:raise ValueError('invalid_flow_response')
    Pin=Q/response;u*=Pin;p*=Pin
    assert np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    reactions=[A@u[a]+blocks[a].T@p-(f*Pin if a==0 else 0) for a in range(3)]
    force=[float(-r[ub.get_dofs('body').all()].sum()) for r in reactions] if body else [0.]*3
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)] if body else []
    if body:
        for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
            assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    g=gradients(ub);div=np.einsum('ii...->...',g);e=.5*(g+g.swapaxes(0,1))
    D=float(np.sum(2*mu*np.einsum('ij...,ij...->...',e,e)*ub.dx))
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    consistency=diagnose(mesh,ub,pb,u,p,mu,lo,hi) if body else None
    if snapshot is not None:
        assert not snapshot.exists()
        np.savez_compressed(snapshot,vertices_m=mesh.p,tetrahedra=mesh.t,
            velocity_coefficients=u,pressure_coefficients=p,velocity_doflocs_m=ub.doflocs,
            pressure_doflocs_m=pb.doflocs,lo=lo,hi=hi,mu=mu,length=length)
    return dict(schema='physics_sim_c3d_solenoidal_reference_v1',method='continuous P3 / DG P2 on Alfeld tetrahedra',
        length=length,body=body,body_count=count,split_first_normal=split,n=n,axis_nodes_m=[x.tolist() for x in axes],
        **dimensions,flow_m3_s=Q,mu=mu,inlet_pressure_pa=Pin,free_relative_residual=residual,
        pressure_force_n=checks[0]['pressure_force_n'] if body else [0.]*3,
        raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'] if body else [0.]*3,
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'] if body else [0.]*3,
        reaction_force_n=force,traction_checks=checks,consistency_diagnostics=consistency,
        physical_dissipation_w=D,physical_boundary_power_w=power,physical_energy_imbalance=abs(power-D)/D,
        divergence_l2_s_inv_m_3_2=float(np.sum(div**2*ub.dx))**.5,
        volume_divergence_max_s_inv=float(np.max(np.abs(div))),flux_error=abs(flux(fi)+Q)/Q,
        iterations=iterations[0],timings=timings,wall_s=time.monotonic()-started,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,physical_accuracy_certified=False)


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=2)
    ap.add_argument('--n',type=int,choices=(2,4,8),default=4)
    ap.add_argument('--empty',action='store_true');ap.add_argument('--split',action='store_true')
    ap.add_argument('--mesh-only',action='store_true');ap.add_argument('--snapshot',type=Path)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists()
    row=reference(a.length,not a.empty,a.count,a.split,a.n,a.mesh_only,a.snapshot)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row),flush=True)
