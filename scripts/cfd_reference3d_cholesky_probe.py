#!/usr/bin/env python3
"""Bounded exact-condensed cube solve; original FE quadrature verifies full fields."""
import argparse
import json
import time
import resource
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import minres,gmres,LinearOperator,splu
from skfem import FacetBasis,LinearForm,asm
from cfd_reference3d_accelerate import CholeskyFactor
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_condensed_pc import symmetric_ilu,coupled_ilu,component_ssor,coupled_amg
from cfd_reference3d_graded_mesh import graded_mesh
from cfd_reference3d_quartic_pair import CubicPressureMass
from cfd_reference3d_quartic_observation import quartic_consistency
from cfd_reference3d_preconditioner import array_sha,matrix_sha
from cfd_reference3d_chunked import volume_metrics
from cfd_reference3d_traction import traction
from cfd_reference3d_graded_probe import numerical_failure_reasons,publish_snapshot


def run(length=4.,count=2,split=False,exponent=3,outer_layers=1,target=1e-10,maxiter=3000,kind='coupled_cholesky',snapshot=None,chunk_size=128,drop_tol=1e-4,fill_factor=8.,ssor_cycles=1,factor_library=None,factor_order='metis',cache_mib=0):
    started=time.monotonic();mu=.1;Q=.008;resource_samples={}
    def sample(phase):
        resource_samples[phase]=dict(wall_s=time.monotonic()-started,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        print(json.dumps(dict(phase=phase,**resource_samples[phase])),flush=True)
    mesh,lo,hi,axes,macros=graded_mesh(length,count,split,exponent,outer_layers)
    print(json.dumps({'phase':'mesh','tetrahedra':mesh.nelements,'macro_tetrahedra':macros}),flush=True)
    system=CondensedSystem(mesh,mu,cache_cap_bytes=cache_mib*1024**2);ub,pb=system.ub,system.pb
    fixed=ub.get_dofs(['walls','body']).all();fullfree=np.setdiff1d(np.arange(ub.N),fixed)
    fixed_trace=system.trace_inverse[fixed];assert np.all(fixed_trace>=0)
    trace_free=np.setdiff1d(np.arange(system.nt),fixed_trace);nf=len(trace_free);nv=3*nf
    retained_free=np.r_[np.concatenate([trace_free+a*system.nt for a in range(3)]),np.arange(macros)+3*system.nt]
    C=system.matrix[retained_free][:,retained_free];del system.matrix
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=8))
    full_rhs=np.r_[f,np.zeros(2*ub.N+pb.N)]
    rhs=system.reduce_rhs(full_rhs)[retained_free]
    original_rhs=np.r_[f[fullfree],np.zeros(2*len(fullfree)+pb.N)];scale=np.linalg.norm(original_rhs)
    identity=dict(mesh_sha256=array_sha(mesh.p,mesh.t),free_dofs_sha256=array_sha(fullfree),
        rhs_sha256=array_sha(original_rhs),condensed_matrix_sha256=matrix_sha(C),
        matrix_identity_scope='exact condensed operator differs in coordinates; original equation/field equivalence verified separately')
    assembly=time.monotonic()-started
    print(json.dumps({'phase':'assembled',**system.metadata,'identity':identity,'assembly_s':assembly,
        'peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    begin=time.monotonic()
    def factor(matrix):return splu(matrix.tocsc(),permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.,options={'SymmetricMode':True})
    if kind in ('component_factor','component_ssor'):
        factors=[factor(C[a*nf:(a+1)*nf,a*nf:(a+1)*nf]) for a in range(3)]
        def velocity(x):return np.concatenate([fac.solve(x[a*nf:(a+1)*nf]) for a,fac in enumerate(factors)])
        if kind=='component_ssor':
            def velocity_action(x):return (C@np.r_[x,np.zeros(macros)])[:nv]
            velocity=component_ssor(C[:nv,:nv],factors,ssor_cycles,velocity_action)
    elif kind=='coupled_cholesky':
        exact_factor=CholeskyFactor(C,factor_library,factor_order,prefix=nv)
        velocity=exact_factor.solve;factors=[]
    elif kind=='coupled_factor':
        factors=[factor(C[:nv,:nv])]
        def velocity(x):return factors[0].solve(x)
    elif kind=='coupled_amg_gmres':
        velocity,incomplete_meta=coupled_amg(C[:nv,:nv]);factors=[]
    elif kind=='coupled_ilu_gmres':
        velocity,incomplete_meta=coupled_ilu(C[:nv,:nv],drop_tol=drop_tol,fill_factor=fill_factor)
        factors=[]
    else:
        velocity,incomplete_meta=symmetric_ilu(C[:nv,:nv],drop_tol=drop_tol,fill_factor=fill_factor)
        factors=[]
    meta=dict(kind=kind,factor_storage_nnz=sum(int(f.nnz) for f in factors),
        scope='SPD condensed velocity block/component factors and exact macro-constant pressure mass; unchanged physical equations')
    if kind=='coupled_cholesky':meta.pop('factor_storage_nnz');meta.update(exact_factor.metadata)
    if kind=='component_ssor':meta.update(ssor_cycles=ssor_cycles,scope='fixed repeated block SSOR inverse; SPD because SSOR majorizes the physical SPD velocity block; unchanged physical operator')
    if kind in ('symmetric_ilu','coupled_ilu_gmres','coupled_amg_gmres'):meta.update(incomplete_meta)
    def precondition(x):return np.r_[velocity(x[:nv]),mu*x[nv:]/system.volumes]
    setup=time.monotonic()-begin;sample('factor_ready')
    print(json.dumps({'phase':'solve','preconditioner':meta,'setup_s':setup}),flush=True)
    progress=[];iterations=[0]
    def retained_residual(x):
        r=C@x-rhs
        return dict(estimated_full_residual=float(np.sqrt(r[:nv]@r[:nv]+(r[nv:]@r[nv:])/80)/scale),
            retained_momentum_relative_to_original_rhs=float(np.linalg.norm(r[:nv])/scale),
            retained_constant_continuity_relative_to_original_rhs=float(np.linalg.norm(r[nv:])/np.sqrt(80)/scale),
            scope='retained-row original scaling estimate; does not include local elimination roundoff and is not full residual authority')
    class Converged(Exception):
        def __init__(self,x):self.x=x.copy()
    def callback(x):
        iterations[0]+=1
        if iterations[0]%10==0:
            row=retained_residual(x)
            if iterations[0]%100==0:
                row['iteration']=iterations[0];progress.append(row);print(json.dumps(row),flush=True)
            if row['estimated_full_residual']<=target:raise Converged(x)
    begin=time.monotonic()
    if kind in ('coupled_ilu_gmres','coupled_amg_gmres'):
        def gmres_callback(norm):
            iterations[0]+=1
            if iterations[0]%100==0:
                item=dict(iteration=iterations[0],gmres_preconditioned_residual=float(norm),
                    scope='GMRES progress only; independent reconstructed full residual remains authority')
                progress.append(item);print(json.dumps(item),flush=True)
        z,info=gmres(C,rhs,M=LinearOperator(C.shape,precondition),rtol=target*.1,
            restart=50,maxiter=maxiter,callback=gmres_callback,callback_type='legacy')
    else:
        try:z,info=minres(C,rhs,M=LinearOperator(C.shape,precondition),rtol=1e-14,maxiter=maxiter,callback=callback)
        except Converged as result:z,info=result.x,0
    solve_s=time.monotonic()-begin;estimate=retained_residual(z);sample('solve_complete')
    if kind=='coupled_cholesky':
        exact_factor.close();del exact_factor;del velocity
        sample('factor_released')
    full_retained=np.zeros(system.shape[0]);full_retained[retained_free]=z
    begin=time.monotonic();u,p=system.reconstruct(full_retained,full_rhs)
    action=full_action(mesh,ub,pb,u,p,mu,chunk_size)
    original_free=np.r_[np.concatenate([fullfree+a*ub.N for a in range(3)]),np.arange(pb.N)+3*ub.N]
    r=(action-full_rhs)[original_free];nfullv=3*len(fullfree);mass=CubicPressureMass(pb,mu)
    residual=dict(true_residual=float(np.linalg.norm(r)/scale),momentum_relative_to_rhs=float(np.linalg.norm(r[:nfullv])/scale),
        continuity_relative_to_rhs=float(np.linalg.norm(r[nfullv:])/scale),continuum_divergence_l2_unit_response=mass.dual_norm(r[nfullv:]),
        pressure_mass_norm=mass.norm(p),authority='independent original P4/DG-P3 full FE quadrature after reconstruction')
    reconstruction_s=time.monotonic()-begin;sample('full_residual_verified')
    linear_accepted=info==0 and residual['true_residual']<1e-8
    condition=np.linalg.cond(mesh.mapping().A.transpose(2,0,1))
    row=dict(schema='physics_sim_c3d_cholesky_probe_v1',velocity_degree=4,pressure_degree=3,verified_volume_product_degree=6,
        linear_solve_accepted=linear_accepted,numerically_accepted=False,physical_accuracy_certified=False,
        info=int(info),iterations=iterations[0],iteration_limit=maxiter,target=target,
        final_residual=residual,final_retained_residual=estimate,identity=identity,progress=progress,
        preconditioner=meta,pressure_preconditioner={'kind':'exact_macro_constant_mass'},condensation=system.metadata,
        diagnostics=dict(Jacobian_condition_median=float(np.median(condition)),Jacobian_condition_max=float(condition.max())),
        timings=dict(assembly_s=assembly,setup_s=setup,solve_s=solve_s,reconstruction_and_full_residual_s=reconstruction_s),
        resource_samples=resource_samples,chunk_size=chunk_size,
        length=length,count=count,split_first_normal=split,body=True,outer_grading_exponent=exponent,outer_layers=outer_layers,
        tetrahedra=mesh.nelements,velocity_dofs=int(3*ub.N),pressure_dofs=int(pb.N),condensed_free_dofs=C.shape[0])
    if not linear_accepted:
        row.update(numerical_failure_reasons=['full_linear_residual'],peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-started)
        return row
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=8)
    def values(b):return np.stack([b.interpolate(v) for v in u])
    def gradients(b):return np.stack([b.interpolate(v).grad for v in u])
    fi,fo=facet('inlet'),facet('outlet')
    def flux(b):return float(np.sum(np.einsum('i...,i...->...',values(b),b.normals)*b.dx))
    response=flux(fo);assert np.isfinite(response) and response>0
    Pin=Q/response;u*=Pin;p*=Pin
    assert np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    D,div_l2,div_max=volume_metrics(mesh,ub,u,mu,chunk_size)
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)]
    for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
        assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    body=ub.get_dofs('body').all();momentum=(action-full_rhs)[:3*ub.N].reshape(3,-1)
    reaction=(-Pin*momentum[:,body].sum(axis=1)).tolist()
    row.update(inlet_pressure_pa=Pin,flow_m3_s=Q,mu=mu,axis_nodes_m=[a.tolist() for a in axes],
        pressure_force_n=checks[0]['pressure_force_n'],raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'],
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'],reaction_force_n=reaction,traction_checks=checks,
        consistency_diagnostics=quartic_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size),
        physical_dissipation_w=D,physical_boundary_power_w=power,physical_energy_imbalance=abs(power-D)/D,
        divergence_l2_s_inv_m_3_2=div_l2,volume_divergence_max_s_inv=div_max,flux_error=abs(flux(fi)+Q)/Q,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-started)
    sample('physical_diagnostics_complete')
    row['numerical_failure_reasons']=numerical_failure_reasons(row);row['numerically_accepted']=not row['numerical_failure_reasons']
    if snapshot is not None and row['numerically_accepted']:
        publish_snapshot(snapshot,dict(vertices_m=mesh.p,tetrahedra=mesh.t,velocity_coefficients=u,
            pressure_coefficients=p,velocity_doflocs_m=ub.doflocs,pressure_doflocs_m=pb.doflocs,
            lo=lo,hi=hi,mu=mu,length=length,velocity_degree=4,pressure_degree=3),row,started)
    sample('publication_complete')
    return row


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.);ap.add_argument('--count',type=int,choices=(2,4,6),default=2)
    ap.add_argument('--split',action='store_true');ap.add_argument('--exponent',type=int,choices=(1,2,3),default=3)
    ap.add_argument('--outer-layers',type=int,choices=(1,2,3),default=1)
    ap.add_argument('--target',type=float,choices=(1e-10,1e-12),default=1e-10);ap.add_argument('--maxiter',type=int,default=3000)
    ap.add_argument('--kind',choices=('component_factor','component_ssor','coupled_factor','coupled_cholesky','symmetric_ilu','coupled_ilu_gmres','coupled_amg_gmres'),default='coupled_cholesky')
    ap.add_argument('--drop-tol',type=float,choices=(1e-4,1e-3),default=1e-4);ap.add_argument('--fill-factor',type=float,choices=(3.,5.,8.),default=8.)
    ap.add_argument('--chunk-size',type=int,choices=(128,512),default=128)
    ap.add_argument('--cache-mib',type=int,choices=(0,64),default=0)
    ap.add_argument('--factor-order',choices=('amd','metis'),default='metis')
    ap.add_argument('--factor-library',type=Path,required=True)
    ap.add_argument('--ssor-cycles',type=int,choices=(1,2,4),default=1)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and 1<=a.maxiter<=3000
    row=run(a.length,a.count,a.split,a.exponent,a.outer_layers,a.target,a.maxiter,a.kind,a.snapshot,drop_tol=a.drop_tol,fill_factor=a.fill_factor,ssor_cycles=a.ssor_cycles,factor_library=a.factor_library,chunk_size=a.chunk_size,factor_order=a.factor_order,cache_mib=a.cache_mib)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({'numerically_accepted':row['numerically_accepted'],
        'iterations':row['iterations'],'final_residual':row['final_residual']}),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
