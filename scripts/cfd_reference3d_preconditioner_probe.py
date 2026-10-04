#!/usr/bin/env python3
"""Exact reference cube preconditioner controls with separate residual diagnostics."""
import argparse
import json
import time
from pathlib import Path
import numpy as np
from scipy.sparse import hstack
from scipy.sparse.linalg import LinearOperator, minres
from skfem import FacetBasis, LinearForm, asm
from cfd_fem_reference3d_solenoidal import mesh_for_case
from cfd_reference3d_stokes_pair import assemble_pair,mixed_matrix
from cfd_reference3d_preconditioner import (PressureMass,velocity_preconditioner,
    macro_pressure_modes,residual_diagnostics,array_sha,matrix_sha)
from cfd_reference3d_traction import traction
from cfd_reference3d_consistency import diagnose


def run(kind='amg',maxiter=3000,length=4.,count=2,split=False,empty=False,modes=False,target=1e-10,snapshot=None):
    assert 1<=maxiter<=3000 and target in (1e-10,1e-12)
    started=time.monotonic();mu=.1;Q=.008
    mesh,lo,hi,axes,macro_count=mesh_for_case(length,not empty,count,split,n=4)
    ub,pb,A,blocks=assemble_pair(mesh,mu)
    fixed=ub.get_dofs(['walls']+(['body'] if not empty else [])).all()
    free=np.setdiff1d(np.arange(ub.N),fixed);Af=A[free][:,free]
    K=mixed_matrix(A,blocks,free);B=hstack([b[:,free] for b in blocks],format='csr')
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=6))
    nv=3*len(free);rhs=np.r_[f[free],np.zeros(2*len(free)+pb.N)]
    identity={'mesh_sha256':array_sha(mesh.p,mesh.t),'free_dofs_sha256':array_sha(free),
        'matrix_sha256':matrix_sha(K),'rhs_sha256':array_sha(rhs)}
    mass=PressureMass(pb,mu);assembly=time.monotonic()-started
    modal=macro_pressure_modes(B,Af,pb,macro_count,mu) if modes else None
    mapping=mesh.mapping().A.transpose(2,0,1);condition=np.linalg.cond(mapping)
    diagnostics={'pressure_modes':modal,'Jacobian_condition_median':float(np.median(condition)),
        'Jacobian_condition_max':float(condition.max())}
    print(json.dumps({'phase':'assembled','tetrahedra':mesh.nelements,'matrix_nnz':K.nnz,
        'velocity_free_dofs':nv,'pressure_dofs':int(pb.N),'identity':identity,'diagnostics':diagnostics}),flush=True)
    begin=time.monotonic();velocity,pc_meta=velocity_preconditioner(Af,kind)
    def precondition(x):
        return np.r_[np.concatenate([velocity(v) for v in x[:nv].reshape(3,-1)]),mass.precondition(x[nv:])]
    setup=time.monotonic()-begin
    print(json.dumps({'phase':'solve','preconditioner':pc_meta,'assembly_s':assembly,'setup_s':setup}),flush=True)
    iterations=[0];progress=[]
    class Converged(Exception):
        def __init__(self,x):self.x=x.copy()
    def callback(x):
        iterations[0]+=1
        if iterations[0]%10==0:
            row=residual_diagnostics(K,x,rhs,nv,mass)
            if iterations[0]%100==0:
                row['iteration']=iterations[0];progress.append(row);print(json.dumps(row),flush=True)
            if row['true_residual']<=target:raise Converged(x)
    begin=time.monotonic()
    try:
        z,info=minres(K,rhs,M=LinearOperator(K.shape,precondition),rtol=1e-14,maxiter=maxiter,callback=callback)
    except Converged as accepted:z,info=accepted.x,0
    residual=residual_diagnostics(K,z,rhs,nv,mass);solve_s=time.monotonic()-begin
    accepted=info==0 and residual['true_residual']<1e-8
    row={'schema':'physics_sim_c3d_preconditioner_probe_v1','preconditioner':pc_meta,
        'numerically_accepted':accepted,'physical_accuracy_certified':False,'info':int(info),
        'iterations':iterations[0],'iteration_limit':maxiter,'target':target,'final_residual':residual,
        'identity':identity,'diagnostics':diagnostics,'progress':progress,
        'timings':{'assembly_s':assembly,'setup_s':setup,'solve_s':solve_s},
        'length':length,'count':count,'split_first_normal':split,'body':not empty,
        'tetrahedra':mesh.nelements,'velocity_dofs':int(3*ub.N),'pressure_dofs':int(pb.N)}
    if not accepted:
        row['wall_s']=time.monotonic()-started
        return row
    u=np.zeros((3,ub.N));u[:,free]=z[:nv].reshape(3,-1);p=z[nv:]
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=6)
    def values(b):return np.stack([b.interpolate(v) for v in u])
    def gradients(b):return np.stack([b.interpolate(v).grad for v in u])
    fi,fo=facet('inlet'),facet('outlet')
    def flux(b):return float(np.sum(np.einsum('i...,i...->...',values(b),b.normals)*b.dx))
    response=flux(fo);assert np.isfinite(response) and response>0
    Pin=Q/response;u*=Pin;p*=Pin
    assert np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    g=gradients(ub);div=np.einsum('ii...->...',g);e=.5*(g+g.swapaxes(0,1))
    D=float(np.sum(2*mu*np.einsum('ij...,ij...->...',e,e)*ub.dx))
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)] if not empty else []
    for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
        if checks:assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    reaction=[float(-(A@u[a]+blocks[a].T@p-(f*Pin if a==0 else 0))[ub.get_dofs('body').all()].sum()) for a in range(3)] if not empty else [0.]*3
    row.update(inlet_pressure_pa=Pin,flow_m3_s=Q,mu=mu,axis_nodes_m=[a.tolist() for a in axes],
        pressure_force_n=checks[0]['pressure_force_n'] if checks else [0.]*3,
        raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'] if checks else [0.]*3,
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'] if checks else [0.]*3,
        reaction_force_n=reaction,traction_checks=checks,
        consistency_diagnostics=diagnose(mesh,ub,pb,u,p,mu,lo,hi) if not empty else None,
        physical_dissipation_w=D,physical_boundary_power_w=power,physical_energy_imbalance=abs(power-D)/D,
        divergence_l2_s_inv_m_3_2=float(np.sum(div**2*ub.dx))**.5,
        volume_divergence_max_s_inv=float(np.max(np.abs(div))),flux_error=abs(flux(fi)+Q)/Q)
    if snapshot is not None:
        assert not snapshot.exists()
        np.savez_compressed(snapshot,vertices_m=mesh.p,tetrahedra=mesh.t,velocity_coefficients=u,
            pressure_coefficients=p,velocity_doflocs_m=ub.doflocs,pressure_doflocs_m=pb.doflocs,
            lo=lo,hi=hi,mu=mu,length=length,numerically_accepted=True)
    row['wall_s']=time.monotonic()-started
    return row


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kind',choices=('amg','factor'),default='amg')
    ap.add_argument('--maxiter',type=int,default=3000);ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=2);ap.add_argument('--split',action='store_true')
    ap.add_argument('--empty',action='store_true');ap.add_argument('--modes',action='store_true')
    ap.add_argument('--target',type=float,choices=(1e-10,1e-12),default=1e-10)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists()
    row=run(a.kind,a.maxiter,a.length,a.count,a.split,a.empty,a.modes,a.target,a.snapshot)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
