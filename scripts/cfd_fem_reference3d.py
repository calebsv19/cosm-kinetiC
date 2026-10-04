#!/usr/bin/env python3
"""Independent continuous P2/P1 tetrahedral Stokes cube reference (not native code)."""
import argparse,json,time,resource
from pathlib import Path
import numpy as np
from scipy.sparse import bmat
from scipy.sparse.linalg import LinearOperator,minres
from skfem import MeshTet,ElementTetP2,ElementTetP1,ElementVector,Basis,FacetBasis,Functional,LinearForm,asm
from skfem.models.poisson import vector_laplace,mass
from skfem.models.general import divergence
from pyamg import smoothed_aggregation_solver
import skfem,scipy,pyamg


def reference(n,L=4.,cx=2.,levels=0,body=True,mu=.1,Q=.008,edge_only=False):
    started=time.monotonic()
    mesh=MeshTet.init_tensor(np.linspace(0,L,round(L*n/2)+1),np.linspace(0,2,n+1),np.linspace(0,2,n+1))
    lo=np.array([cx-.5,.5,.5]);hi=lo+1
    if body:
        center=mesh.p[:,mesh.t].mean(axis=1)
        mesh=mesh.remove_elements(np.nonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0))[0])
        for level in range(levels):
            c=mesh.p[:,mesh.t].mean(axis=1)
            # Refine near the body; exact polyhedral surface, no changing geometry.
            dist=np.linalg.norm(np.maximum(np.maximum(lo[:,None]-c,c-hi[:,None]),0),axis=0)
            if edge_only:
                distances=[]
                for axis in range(3):
                    others=[a for a in range(3) if a!=axis]
                    for sa in (0,1):
                        for sb in (0,1):
                            d=np.maximum(np.maximum(lo[axis]-c[axis],c[axis]-hi[axis]),0)**2
                            d+=(c[others[0]]-(hi if sa else lo)[others[0]])**2
                            d+=(c[others[1]]-(hi if sb else lo)[others[1]])**2
                            distances.append(np.sqrt(d))
                dist=np.min(distances,axis=0)
            mesh=mesh.refined(np.nonzero(dist<(2/n/(2**(level/3)) if edge_only else 2/n/(2**level)))[0])
    assert mesh.nelements <= 50000, ("reference_mesh_cap",mesh.nelements)
    def body_surface(x):
        within=np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)
        return within&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],L),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),
        'body':body_surface})
    ub=Basis(mesh,ElementVector(ElementTetP2()),intorder=2);pb=Basis(mesh,ElementTetP1(),intorder=2)
    A=mu*asm(vector_laplace,ub);B=-asm(divergence,ub,pb)
    @LinearForm
    def inflow(v,w):return v[0]
    f=asm(inflow,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=4))
    fixed=ub.get_dofs(['walls']+(['body'] if body else [])).all()
    free=np.setdiff1d(np.arange(ub.N),fixed);Af=A[free][:,free];Bf=B[:,free]
    K=bmat([[Af,Bf.T],[Bf,None]],format='csr');rhs=np.r_[f[free],np.zeros(pb.N)]
    # Components are uncoupled in A: one scalar hierarchy per component, no dense factorization.
    amg=smoothed_aggregation_solver(Af,max_coarse=100,symmetry='symmetric',
        presmoother=('gauss_seidel',{'sweep':'symmetric'}),postsmoother=('gauss_seidel',{'sweep':'symmetric'}))
    pc=amg.aspreconditioner();diagonal=asm(mass,pb).diagonal()/mu
    M=LinearOperator(K.shape,lambda x:np.r_[pc@x[:len(free)],x[len(free):]/diagonal])
    its=[0]
    def callback(x):its[0]+=1
    z,info=minres(K,rhs,M=M,rtol=1e-12,maxiter=3000,callback=callback)
    rel=np.linalg.norm(K@z-rhs)/np.linalg.norm(rhs)
    assert info==0 and rel<1e-8,(info,rel)
    u=np.zeros(ub.N);u[free]=z[:len(free)];p=z[len(free):]
    @Functional
    def flux(w):return np.einsum('i...,i...->...',w.u,w.n)
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=4)
    fi=facet('inlet');fo=facet('outlet')
    qout=float(asm(flux,fo,u=fo.interpolate(u)));scale=Q/qout;u*=scale;p*=scale;Pin=scale
    residual=A@u+B.T@p-f*scale
    pressure_force=[];viscous_force=[];raw_viscous_force=[]
    if body:
        fb=facet('body');fp=FacetBasis(mesh,pb.elem,facets=mesh.boundaries['body'],intorder=4)
        ui=fb.interpolate(u);pi=fp.interpolate(p)
        for a in range(3):
            @Functional
            def pf(w):return w.p*w.n[a]
            @Functional
            def vf(w):return -mu*np.einsum('j...,j...->...',w.u.grad[a]+w.u.grad[:,a],w.n)
            pressure_force.append(float(asm(pf,fb,p=pi)))
            raw_viscous_force.append(float(asm(vf,fb,u=ui)))
            @Functional
            def wall_consistent(w):
                # Exact stationary flat no-slip trace has zero tangential
                # derivatives; div u=0 then fixes the normal derivative to zero.
                # Keep the tangential normal gradient, not weak-FEM div error.
                return -mu*np.einsum('j...,j...->...',w.u.grad[a],w.n)*(1-w.n[a]**2)
            viscous_force.append(float(asm(wall_consistent,fb,u=ui)))
        reaction=[float(-residual[ub.get_dofs('body').all(['u^'+str(a+1)])].sum()) for a in range(3)]
    else:reaction=[0,0,0];pressure_force=[0,0,0];viscous_force=[0,0,0]
    @Functional
    def dissipation(w):
        g=w.u.grad;eps=.5*(g+g.swapaxes(0,1));return 2*mu*np.einsum('ij...,ij...->...',eps,eps)
    @Functional
    def extra_work(w):return mu*np.einsum('i...,ji...,j...->...',w.u,w.u.grad,w.n)
    D=float(asm(dissipation,ub,u=ub.interpolate(u)))
    boundary=Pin*Q+float(asm(extra_work,fi,u=fi.interpolate(u)))+float(asm(extra_work,fo,u=fo.interpolate(u)))
    out={'schema':'physics_sim_c3d8_fem_reference_v1','n':n,'length':L,'center_x':cx,'body':body,'levels':levels,'edge_only':edge_only,
         'mu':mu,'flow_m3_s':Q,'inlet_pressure_pa':Pin,'pressure_force_n':pressure_force,'viscous_force_n':viscous_force,
         'raw_symmetric_viscous_force_n':raw_viscous_force,'reaction_force_n':reaction,'physical_dissipation_w':D,'physical_boundary_power_w':boundary,
         'physical_energy_imbalance':abs(boundary-D)/D,'free_relative_residual':float(rel),
         'flux_error':abs(float(asm(flux,fi,u=fi.interpolate(u)))+Q)/Q,'tetrahedra':int(mesh.nelements),
         'velocity_dofs':int(ub.N),'pressure_dofs':int(pb.N),'iterations':its[0],'wall_s':time.monotonic()-started,
         'peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
         'versions':{'skfem':skfem.__version__,'scipy':scipy.__version__,'numpy':np.__version__,'pyamg':pyamg.__version__}}
    return out

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--n',type=int,default=8);ap.add_argument('--length',type=float,default=4)
    ap.add_argument('--center',type=float,default=2);ap.add_argument('--levels',type=int,default=0)
    ap.add_argument('--empty',action='store_true');ap.add_argument('--edges-only',action='store_true');ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();row=reference(a.n,a.length,a.center,a.levels,not a.empty,edge_only=a.edges_only)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row),flush=True)
