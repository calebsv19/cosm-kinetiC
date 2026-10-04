#!/usr/bin/env python3
"""Exact reference cube preconditioner controls with separate residual diagnostics."""
import argparse
import json
import time
import resource
from types import SimpleNamespace
from pathlib import Path
import numpy as np
from scipy.sparse import hstack
from scipy.sparse.linalg import LinearOperator, minres
from skfem import FacetBasis, LinearForm, asm
from cfd_reference3d_spatial_mesh import spatial_mesh
from cfd_reference3d_stokes_pair import assemble_pair,mixed_matrix
from cfd_reference3d_preconditioner import (PressureMass,velocity_preconditioner,
    macro_pressure_modes,residual_diagnostics,array_sha,matrix_sha)
from cfd_reference3d_chunked import assemble_chunked,chunked_mass,volume_metrics,diagnose_chunked,streamed_matrix_sha,reaction_weights,mixed_matrix_lean,MixedOperator,saddle_digest
from cfd_reference3d_pressure_modes import local_macro_pressure_modes,macro_pressure_preconditioner,patch_pressure_preconditioner
from cfd_reference3d_traction import traction
from cfd_reference3d_consistency import diagnose


def run(kind='amg',maxiter=3000,length=4.,count=2,split=False,empty=False,modes=False,target=1e-10,snapshot=None,normal_spacing=None,insert_normal=False,chunk_size=0,matrix_free=False,local_modes=False,pressure_kind='mass',edge_passes=0,edge_radius=.12,mesh_data=None):
    assert not matrix_free or chunk_size>0
    assert 1<=maxiter<=3000 and target in (1e-10,1e-12)
    started=time.monotonic();mu=.1;Q=.008
    mesh,lo,hi,axes,macro_count=mesh_data if mesh_data is not None else spatial_mesh(length,not empty,count,split,n=4,normal_spacing=normal_spacing,insert_normal=insert_normal,edge_passes=edge_passes,edge_radius=edge_radius)
    ub,pb,A,blocks=assemble_chunked(mesh,mu,chunk_size) if chunk_size else assemble_pair(mesh,mu)
    print(json.dumps({'phase':'forms','peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    fixed=ub.get_dofs(['walls']+(['body'] if not empty else [])).all()
    free=np.setdiff1d(np.arange(ub.N),fixed);Af=A[free][:,free]
    mass=chunked_mass(pb,mu) if chunk_size else PressureMass(pb,mu)
    local=local_macro_pressure_modes(mesh,ub,pb,A,blocks,mass,macro_count,mu) if local_modes else None
    pressure_setup_start=time.monotonic()
    pressure_factory={'macro':macro_pressure_preconditioner,'patch':patch_pressure_preconditioner}.get(pressure_kind)
    pressure,pressure_meta=pressure_factory(mesh,ub,pb,A,blocks,mass,free,macro_count,mu) if pressure_factory else (mass.precondition,{'kind':'mass'})
    pressure_setup=time.monotonic()-pressure_setup_start
    weights=reaction_weights(A,blocks,ub.get_dofs('body').all()) if chunk_size and not empty else None
    if chunk_size:
        B=hstack([b[:,free] for b in blocks],format='csr')
        del A,blocks
        digest=saddle_digest(Af,B) if matrix_free else None
        K=MixedOperator.build(Af,B) if matrix_free else mixed_matrix_lean(Af,B)
        if not modes:B=None
    else:
        K=mixed_matrix(A,blocks,free);B=hstack([b[:,free] for b in blocks],format='csr') if modes else None
    print(json.dumps({'phase':'mixed-matrix','peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=6))
    nv=3*len(free);rhs=np.r_[f[free],np.zeros(2*len(free)+pb.N)]
    identity={'mesh_sha256':array_sha(mesh.p,mesh.t),'free_dofs_sha256':array_sha(free),
        'matrix_sha256':digest if matrix_free else (streamed_matrix_sha(K) if chunk_size else matrix_sha(K)),'rhs_sha256':array_sha(rhs)}
    assembly=time.monotonic()-started
    modal_pb=SimpleNamespace(N=pb.N,dx=np.abs(mesh.mapping().detA)[:,None]/6) if chunk_size else pb
    modal=macro_pressure_modes(B,Af,modal_pb,macro_count,mu) if modes else None
    mapping=mesh.mapping().A.transpose(2,0,1);condition=np.linalg.cond(mapping)
    diagnostics={'pressure_modes':modal,'local_macro_pressure_modes':local,'Jacobian_condition_median':float(np.median(condition)),
        'Jacobian_condition_max':float(condition.max())}
    print(json.dumps({'phase':'assembled','tetrahedra':mesh.nelements,'matrix_nnz':K.nnz,
        'velocity_free_dofs':nv,'pressure_dofs':int(pb.N),'identity':identity,'diagnostics':diagnostics}),flush=True)
    if B is not None:del B
    begin=time.monotonic();velocity,pc_meta=velocity_preconditioner(Af,kind)
    if chunk_size:del Af
    def precondition(x):
        return np.r_[np.concatenate([velocity(v) for v in x[:nv].reshape(3,-1)]),pressure(x[nv:])]
    setup=time.monotonic()-begin+pressure_setup
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
    row={'schema':'physics_sim_c3d_preconditioner_probe_v1','preconditioner':pc_meta,'pressure_preconditioner':pressure_meta,
        'numerically_accepted':accepted,'physical_accuracy_certified':False,'info':int(info),
        'iterations':iterations[0],'iteration_limit':maxiter,'target':target,'final_residual':residual,
        'identity':identity,'diagnostics':diagnostics,'progress':progress,
        'timings':{'assembly_s':assembly,'setup_s':setup,'solve_s':solve_s},
        'edge_passes':edge_passes,'edge_radius_m':edge_radius,'matrix_free':matrix_free,'chunk_size':chunk_size,'insert_normal':insert_normal,'normal_spacing_m':normal_spacing,'length':length,'count':count,'split_first_normal':split,'body':not empty,
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
    if chunk_size:
        D,div_l2,div_max=volume_metrics(mesh,ub,u,mu,chunk_size)
    else:
        g=gradients(ub);div=np.einsum('ii...->...',g);e=.5*(g+g.swapaxes(0,1))
        D=float(np.sum(2*mu*np.einsum('ij...,ij...->...',e,e)*ub.dx))
        div_l2=float(np.sum(div**2*ub.dx))**.5;div_max=float(np.max(np.abs(div)))
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)] if not empty else []
    for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
        if checks:assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    if chunk_size and not empty:
        reaction=[float(-(weights[0]@u[a]+weights[1][a]@p-(f[ub.get_dofs('body').all()].sum()*Pin if a==0 else 0))) for a in range(3)]
    else:
        reaction=[float(-(A@u[a]+blocks[a].T@p-(f*Pin if a==0 else 0))[ub.get_dofs('body').all()].sum()) for a in range(3)] if not empty else [0.]*3
    row.update(inlet_pressure_pa=Pin,flow_m3_s=Q,mu=mu,axis_nodes_m=[a.tolist() for a in axes],
        pressure_force_n=checks[0]['pressure_force_n'] if checks else [0.]*3,
        raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'] if checks else [0.]*3,
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'] if checks else [0.]*3,
        reaction_force_n=reaction,traction_checks=checks,
        consistency_diagnostics=(diagnose_chunked(mesh,ub,pb,u,p,mu,lo,hi,chunk_size) if chunk_size else diagnose(mesh,ub,pb,u,p,mu,lo,hi)) if not empty else None,
        physical_dissipation_w=D,physical_boundary_power_w=power,physical_energy_imbalance=abs(power-D)/D,
        divergence_l2_s_inv_m_3_2=div_l2,
        volume_divergence_max_s_inv=div_max,flux_error=abs(flux(fi)+Q)/Q)
    if snapshot is not None:
        assert not snapshot.exists()
        np.savez_compressed(snapshot,vertices_m=mesh.p,tetrahedra=mesh.t,velocity_coefficients=u,
            pressure_coefficients=p,velocity_doflocs_m=ub.doflocs,pressure_doflocs_m=pb.doflocs,
            lo=lo,hi=hi,mu=mu,length=length,numerically_accepted=True)
    row['peak_rss_bytes']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if row['peak_rss_bytes']>1800*1024**2:raise ValueError('own peak RSS cap exceeded')
    row['wall_s']=time.monotonic()-started
    return row


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kind',choices=('amg','factor'),default='amg')
    ap.add_argument('--maxiter',type=int,default=3000);ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=2);ap.add_argument('--split',action='store_true')
    ap.add_argument('--empty',action='store_true');ap.add_argument('--modes',action='store_true')
    ap.add_argument('--target',type=float,choices=(1e-10,1e-12),default=1e-10)
    ap.add_argument('--normal-spacing',type=float)
    ap.add_argument('--insert-normal',action='store_true')
    ap.add_argument('--edge-passes',type=int,choices=(0,1),default=0)
    ap.add_argument('--edge-radius',type=float,default=.12)
    ap.add_argument('--matrix-free',action='store_true')
    ap.add_argument('--local-modes',action='store_true')
    ap.add_argument('--pressure-kind',choices=('mass','macro','patch'),default='mass',help='mass is the working path; macro/patch are retained experimental controls and are not performance improvements')
    ap.add_argument('--chunk-size',type=int,choices=(0,512,1024,2048),default=0)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists()
    row=run(a.kind,a.maxiter,a.length,a.count,a.split,a.empty,a.modes,a.target,a.snapshot,a.normal_spacing,a.insert_normal,a.chunk_size,a.matrix_free,a.local_modes,a.pressure_kind,a.edge_passes,a.edge_radius)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
