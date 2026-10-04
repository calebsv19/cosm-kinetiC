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
from cfd_reference3d_shared_factor import BlockTriangle
from cfd_reference3d_encoded_workspace import EncodedWorkspaceCholesky as VectorWorkspaceCholesky
from cfd_reference3d_fillcomp_ic0 import BalancedIC0,fresh_admission
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_requested_target import requested_full_linear_acceptance
from cfd_reference3d_retained_margin import validate_margin
from cfd_reference3d_pressure_complement10 import build as build_pressure_coarse,reserve as coarse_reserve
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_encoded_operator import EncodedTriangle
from cfd_reference3d_size_selected import select as select_storage
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky as LegacyFactor
from cfd_reference3d_workspace_retirement import capture as capture_workspace,verify as verify_workspace
import weakref
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_sparse_load import SparseLoad
from cfd_reference3d_factor_catalog import CatalogMetadata
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_condensed import full_action
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem as CondensedSystem
from cfd_reference3d_condensed_pc import symmetric_ilu,coupled_ilu,component_ssor,coupled_amg
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped
from cfd_reference3d_quartic_pair import CubicPressureMass
from cfd_reference3d_quartic_observation import quartic_consistency
from cfd_reference3d_fused_observation import observe_fused
from cfd_reference3d_preconditioner import array_sha,matrix_sha
from cfd_reference3d_chunked import volume_metrics
from cfd_reference3d_traction import traction
from cfd_reference3d_graded_probe import numerical_failure_reasons,publish_snapshot


def run(length=4.,count=2,split=False,exponent=3,outer_layers=1,target=1e-10,maxiter=3000,kind='mixed_workspace_coupled_cholesky',snapshot=None,chunk_size=128,drop_tol=1e-4,fill_factor=8.,ssor_cycles=1,factor_library=None,factor_order='metis',cache_mib=0,domain_mesh_mode='original',restart=6,retained_target=1e-11,coarse_pressure='mass',storage_mode='auto'):
    if coarse_pressure not in ('mass','quadratic'):raise ValueError('unsupported declared pressure preconditioner')
    validate_margin(target,retained_target)
    if target!=1e-10 or retained_target!=1e-11:raise ValueError('unsupported declared stopping margin')
    if restart != 6:raise ValueError('unsupported declared restart')
    if kind!='mixed_workspace_coupled_cholesky':raise ValueError('triangle probe requires exact coupled Cholesky')
    started=time.monotonic();mu=.1;Q=.008;resource_samples={}
    def sample(phase):
        resource_samples[phase]=dict(wall_s=time.monotonic()-started,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        print(json.dumps(dict(phase=phase,**resource_samples[phase])),flush=True)
        # Atomic publication owns its post-serialization acceptance check.
        if phase!='publication_complete':enforce_phase(phase,resource_samples[phase]['peak_rss_bytes'],resource_samples[phase]['wall_s'])
        if phase=='workspace_pressure_complete':
            coarse_bytes=coarse_reserve(nv,len(system.volumes)) if coarse_pressure=='quadratic' else 0
            admission=fresh_admission(exact_factor,basis_reservation(C.shape[0],restart),coarse_bytes)
            admission.update(outer_basis_reservation_bytes=basis_reservation(C.shape[0],restart),coarse_pressure_reservation_bytes=coarse_bytes)
            print(json.dumps(dict(phase='numeric_stage_admission',**admission)),flush=True)
            if not admission['numeric_stage_admitted']:
                raise PhaseResourceStopped(dict(phase='numeric_stage_admission',**admission,rss_cap_bytes=1800*1024**2,wall_cap_s=180))
    mesh,lo,hi,axes,macros=domain_mesh(length,count,split,exponent,outer_layers,domain_mesh_mode)
    print(json.dumps({'phase':'mesh','tetrahedra':mesh.nelements,'macro_tetrahedra':macros}),flush=True)
    system=CondensedSystem(mesh,mu,cache_cap_bytes=cache_mib*1024**2,fixed_boundaries=('walls','body'));ub,pb=system.ub,system.pb
    fixed=ub.get_dofs(['walls','body']).all();fullfree=np.setdiff1d(np.arange(ub.N),fixed)
    fixed_trace=system.trace_inverse[fixed];assert np.all(fixed_trace>=0)
    trace_free=np.setdiff1d(np.arange(system.nt),fixed_trace);nf=len(trace_free);nv=3*nf
    retained_free=np.r_[np.concatenate([trace_free+a*system.nt for a in range(3)]),np.arange(macros)+3*system.nt]
    np.testing.assert_array_equal(retained_free,system.retained_free)
    upper=system.upper_matrix;del system.upper_matrix
    C=BlockTriangle(upper,nv)
    del upper
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=8))
    full_rhs=np.r_[f,np.zeros(2*ub.N+pb.N)]
    rhs=system.reduce_rhs(full_rhs)[retained_free]
    original_rhs=np.r_[f[fullfree],np.zeros(2*len(fullfree)+pb.N)];scale=np.linalg.norm(original_rhs)
    identity=dict(mesh_sha256=array_sha(mesh.p,mesh.t),free_dofs_sha256=array_sha(fullfree),
        rhs_sha256=array_sha(original_rhs),stored_block_triangle_sha256={name:matrix_sha(matrix) for name,matrix in (('velocity',C.velocity.upper),('coupling',C.coupling),('pressure',C.pressure.upper))},
        matrix_identity_scope='complete block upper CSR hashes; full mixed action and original FE field authority retained')
    control_vector=np.random.default_rng(1211).normal(size=C.shape[0]);original_action=C@control_vector
    original_mixed_block_nnz=C.stored_nnz
    other_arrays=tuple(a for matrix in (C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))
    other_hash=storage_sha(*other_arrays)
    C.velocity=VectorTriangle(C.velocity.upper,factor_library)
    converted_action=C@control_vector
    action_change=float(np.linalg.norm(converted_action-original_action)/max(np.linalg.norm(original_action),np.finfo(float).tiny))
    if action_change>1e-12 or storage_sha(*other_arrays)!=other_hash:raise ValueError('complete vector conversion changed original mixed action/pressure')
    del original_action,converted_action
    original_vector_sha=C.velocity.input_sha256
    original_triangle=C.velocity;original_double_owner=weakref.ref(original_triangle.values)
    unencoded_action=C@control_vector
    def encoding_check(phase,projected_bytes):
        enforce_phase('exact_encoding_'+phase,max(current_rss_bytes()+projected_bytes,resource.getrusage(resource.RUSAGE_SELF).ru_maxrss),time.monotonic()-started)
    C.velocity,storage_selection=select_storage(original_triangle,factor_library,mode=storage_mode,check=encoding_check)
    SelectedFactor=BalancedIC0
    encoded_action=C@control_vector
    if not np.array_equal(unencoded_action.view(np.uint64),encoded_action.view(np.uint64)) or not original_triangle.input_unchanged() or storage_sha(*other_arrays)!=other_hash:raise ValueError('encoded complete mixed action changed original bits')
    del original_triangle,unencoded_action,encoded_action
    if storage_selection['encoded_selected'] and original_double_owner() is not None:raise ValueError('original Double owner still resident')
    C.velocity.metadata.update(original_double_owner_released=storage_selection['encoded_selected'],full_mixed_action_bitwise_preserved=True)
    C.stored_bytes=C.velocity.stored_bytes+sum(a.nbytes for a in other_arrays)
    identity.update(vector_storage_sha256=original_vector_sha,conversion_full_mixed_relative_action_change=action_change,pressure_coupling_bitwise_preserved=True,
        matrix_identity_scope='original complete scalar block CSR hashes before exact bitwise vector conversion; full mixed action and original FE authority retained')
    system.metadata['block_storage']=dict(original_mixed_scalar_upper_nnz=original_mixed_block_nnz,stored_array_bytes=C.stored_bytes,
        stored_dense_block_value_count=storage_selection['coefficient_count'],velocity=C.velocity.metadata,storage_selection=storage_selection,scope='complete node-block velocity plus unchanged coupling/pressure; no coefficients dropped')
    load=SparseLoad(full_rhs)
    system.metadata['full_load_residency']=load.metadata
    retained_free=system.retained_free
    trace_coords=np.ascontiguousarray(ub.doflocs[:,system.trace[trace_free]].T)
    del original_rhs,full_rhs,f,fullfree,fixed,fixed_trace,trace_free
    suspended=CatalogMetadata(system);suspended.check_detached()
    system.metadata['factor_metadata_residency']=suspended.metadata
    assembly=time.monotonic()-started
    print(json.dumps({'phase':'assembled',**system.metadata,'identity':identity,'assembly_s':assembly,
        'peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    sample('assembly_complete')
    begin=time.monotonic()
    live_arrays=tuple(a for matrix in (C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,load.indices,load.values,system.volumes,retained_free)
    exact_factor=SelectedFactor.__new__(SelectedFactor)
    SelectedFactor.__init__(exact_factor,C.velocity,factor_library,factor_order,live_arrays=live_arrays,action=lambda:C@control_vector,stage_callback=sample,coords=trace_coords,length=length,lo=lo,hi=hi)
    del trace_coords
    velocity=exact_factor.solve
    meta=dict(exact_factor.metadata,operator_representation='complete symmetric velocity/pressure triangles plus both coupling actions; all mixed entries retained')
    coarse=None
    pressure_meta={'kind':'negative_exact_macro_constant_mass_in_upper_triangular_preconditioner'}
    if coarse_pressure=='quadratic':
        coarse=build_pressure_coarse(mesh,system.volumes,mu,length,C.coupling,C.pressure,velocity)
        pressure_meta=coarse.metadata.copy()
        sample('coarse_pressure_ready')
    def precondition(x):
        pressure=-(coarse.apply(x[nv:]) if coarse is not None else mu*x[nv:]/system.volumes)
        return np.r_[velocity(x[:nv]-C.coupling@pressure),pressure]
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
    def callback(x,iteration,true_metric):
        iterations[0]=iteration
        if iteration%100==0:
            row=retained_residual(x);row['iteration']=iteration;progress.append(row);print(json.dumps(row),flush=True)
            sample('flexible_iteration_progress')
    def metric(r):return float(np.sqrt(r[:nv]@r[:nv]+(r[nv:]@r[nv:])/80)/scale)
    begin=time.monotonic()
    z,info,flexible=flexible_gmres(C,rhs,precondition,retained_target,maxiter,restart,metric,callback)
    iterations[0]=flexible['iterations'];meta['flexible_iteration']=flexible
    solve_s=time.monotonic()-begin;estimate=retained_residual(z);sample('solve_complete')
    condensed_free_dofs=C.shape[0];retirement={'performed':False,'reason':'retained solve not strictly converged'}
    retirement_refs=None
    if info==0 and flexible['final_true_metric']<=retained_target:
        if not C.velocity.input_unchanged() or storage_sha(*other_arrays)!=other_hash:raise ValueError('completed mixed operator input changed before retirement')
        retirement_authority=(load.indices,load.values,system.volumes,retained_free,z)
        retirement_authority_sha=storage_sha(*retirement_authority)
        retirement_factor_ref=weakref.ref(exact_factor);retirement_coarse_ref=weakref.ref(coarse) if coarse is not None else None
        retirement_refs,retirement_capture=capture_workspace({
            'factor_predictor_input':exact_factor.rounded,'velocity_coarse_Z':exact_factor.coarse.Z,'velocity_coarse_Y':exact_factor.coarse.Y,'velocity_coarse_factor':exact_factor.coarse.factor[0],
            'velocity_starts':C.velocity.starts,'velocity_rows':C.velocity.rows,**({'velocity_predictor':C.velocity.predictor,'velocity_corrections':C.velocity.corrections} if storage_selection['encoded_selected'] else {'velocity_original_values':C.velocity.values}),
            'coupling_starts':C.coupling.indptr,'coupling_rows':C.coupling.indices,'coupling_values':C.coupling.data,
            'pressure_starts':C.pressure.upper.indptr,'pressure_rows':C.pressure.upper.indices,'pressure_values':C.pressure.upper.data,'pressure_diagonal':C.pressure.diagonal,
            'condensed_rhs':rhs,'control_vector':control_vector},info,flexible['final_true_metric'])
    if kind=='mixed_workspace_coupled_cholesky':
        if not exact_factor.input_unchanged():raise ValueError('solve modified shared physical input')
        meta['shared_input_preserved_after_solve']=True
        exact_factor.close();del exact_factor;del velocity;del coarse
        sample('factor_released')
    if retirement_refs is not None:
        del C,other_arrays,live_arrays,rhs,control_vector,precondition,retained_residual,callback
        if retirement_factor_ref() is not None or (retirement_coarse_ref is not None and retirement_coarse_ref() is not None):raise ValueError('completed factor/coarse owner retained')
        retirement=dict(verify_workspace(retirement_refs,retirement_capture,retirement_authority,retirement_authority_sha),performed=True,factor_and_coarse_owners_released=True)
        sample('completed_workspace_retired')
    metadata_begin=time.monotonic();suspended.restore()
    metadata_restore_s=time.monotonic()-metadata_begin
    system.metadata['factor_metadata_residency']['restoration_wall_s']=metadata_restore_s
    sample('factor_metadata_restored')
    full_rhs=load.materialize()
    fixed=ub.get_dofs(['walls','body']).all();fullfree=np.setdiff1d(np.arange(ub.N),fixed)
    restored_original_rhs=np.r_[full_rhs[:ub.N][fullfree],np.zeros(2*len(fullfree)+pb.N)]
    if array_sha(restored_original_rhs)!=identity['rhs_sha256'] or array_sha(fullfree)!=identity['free_dofs_sha256']:raise ValueError('restored original RHS/free identity changed')
    if float(np.linalg.norm(restored_original_rhs))!=scale:raise ValueError('restored original RHS scaling changed')
    del restored_original_rhs
    system.metadata['full_load_residency']['restored_bitwise_after_factor_cleanup']=True
    full_retained=np.zeros(system.shape[0]);full_retained[retained_free]=z
    begin=time.monotonic();u,p=system.reconstruct(full_retained,full_rhs)
    action=full_action(mesh,ub,pb,u,p,mu,chunk_size)
    original_free=np.r_[np.concatenate([fullfree+a*ub.N for a in range(3)]),np.arange(pb.N)+3*ub.N]
    r=(action-full_rhs)[original_free];nfullv=3*len(fullfree);mass=CubicPressureMass(pb,mu)
    residual=dict(true_residual=float(np.linalg.norm(r)/scale),momentum_relative_to_rhs=float(np.linalg.norm(r[:nfullv])/scale),
        continuity_relative_to_rhs=float(np.linalg.norm(r[nfullv:])/scale),continuum_divergence_l2_unit_response=mass.dual_norm(r[nfullv:]),
        pressure_mass_norm=mass.norm(p),authority='independent original P4/DG-P3 full FE quadrature after reconstruction')
    reconstruction_s=time.monotonic()-begin;sample('full_residual_verified')
    linear_accepted=requested_full_linear_acceptance(info,residual['true_residual'],target)
    condition=np.linalg.cond(mesh.mapping().A.transpose(2,0,1))
    row=dict(schema='physics_sim_c3d_mixed_precision_probe_v1',velocity_degree=4,pressure_degree=3,verified_volume_product_degree=6,
        linear_solve_accepted=linear_accepted,numerically_accepted=False,physical_accuracy_certified=False,
        info=int(info),iterations=iterations[0],iteration_limit=maxiter,target=target,retained_target=retained_target,
        final_residual=residual,final_retained_residual=estimate,identity=identity,progress=progress,
        preconditioner=meta,pressure_preconditioner=pressure_meta,coarse_pressure=coarse_pressure,outer_iteration={'kind':'flexible_right_preconditioned_arnoldi','restart':restart},condensation=system.metadata,
        diagnostics=dict(Jacobian_condition_median=float(np.median(condition)),Jacobian_condition_max=float(condition.max())),
        timings=dict(assembly_s=assembly,setup_s=setup,solve_s=solve_s,metadata_restore_s=metadata_restore_s,reconstruction_and_full_residual_s=reconstruction_s),
        resource_samples=resource_samples,chunk_size=chunk_size,
        domain_mesh_mode=domain_mesh_mode,
        length=length,count=count,split_first_normal=split,body=True,outer_grading_exponent=exponent,outer_layers=outer_layers,
        tetrahedra=mesh.nelements,velocity_dofs=int(3*ub.N),pressure_dofs=int(pb.N),condensed_free_dofs=condensed_free_dofs,workspace_retirement=retirement)
    if not linear_accepted:
        row.update(numerical_failure_reasons=['requested_full_linear_residual'],peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-started)
        return row
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=8)
    def values(b):return np.stack([b.interpolate(v) for v in u])
    def gradients(b):return np.stack([b.interpolate(v).grad for v in u])
    fi,fo=facet('inlet'),facet('outlet')
    def flux(b):return float(np.sum(np.einsum('i...,i...->...',values(b),b.normals)*b.dx))
    response=flux(fo);assert np.isfinite(response) and response>0
    Pin=Q/response;u*=Pin;p*=Pin
    assert np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    (D,div_l2,div_max),consistency=observe_fused(mesh,ub,pb,u,p,mu,lo,hi,chunk_size)
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)]
    for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
        assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    body=ub.get_dofs('body').all();momentum=(action-full_rhs)[:3*ub.N].reshape(3,-1)
    reaction=(-Pin*momentum[:,body].sum(axis=1)).tolist()
    row.update(inlet_pressure_pa=Pin,flow_m3_s=Q,mu=mu,axis_nodes_m=[a.tolist() for a in axes],
        pressure_force_n=checks[0]['pressure_force_n'],raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'],
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'],reaction_force_n=reaction,traction_checks=checks,
        consistency_diagnostics=consistency,
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
    ap.add_argument('--storage-mode',choices=('auto','legacy'),default='auto')
    ap.add_argument('--coarse-pressure',choices=('mass','quadratic'),default='mass')
    ap.add_argument('--restart',type=int,choices=(6,),default=6)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.);ap.add_argument('--count',type=int,choices=(2,4,6),default=2)
    ap.add_argument('--split',action='store_true');ap.add_argument('--exponent',type=int,choices=(1,2,3),default=3)
    ap.add_argument('--outer-layers',type=int,choices=(1,2,3),default=1)
    ap.add_argument('--target',type=float,choices=(1e-10,),default=1e-10);ap.add_argument('--maxiter',type=int,default=3000)
    ap.add_argument('--kind',choices=('mixed_workspace_coupled_cholesky',),default='mixed_workspace_coupled_cholesky')
    ap.add_argument('--drop-tol',type=float,choices=(1e-4,1e-3),default=1e-4);ap.add_argument('--fill-factor',type=float,choices=(3.,5.,8.),default=8.)
    ap.add_argument('--chunk-size',type=int,choices=(128,512),default=128)
    ap.add_argument('--domain-mesh-mode',choices=('original','held_l4'),default='original')
    ap.add_argument('--cache-mib',type=int,choices=(0,64),default=0)
    ap.add_argument('--factor-order',choices=('amd','metis'),default='metis')
    ap.add_argument('--factor-library',type=Path,required=True)
    ap.add_argument('--ssor-cycles',type=int,choices=(1,2,4),default=1)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and 1<=a.maxiter<=3000
    try:
        row=run(a.length,a.count,a.split,a.exponent,a.outer_layers,a.target,a.maxiter,a.kind,a.snapshot,drop_tol=a.drop_tol,fill_factor=a.fill_factor,ssor_cycles=a.ssor_cycles,factor_library=a.factor_library,chunk_size=a.chunk_size,factor_order=a.factor_order,cache_mib=a.cache_mib,domain_mesh_mode=a.domain_mesh_mode,restart=a.restart,coarse_pressure=a.coarse_pressure,storage_mode=a.storage_mode)
    except PhaseResourceStopped as stopped:
        row=dict(schema='physics_sim_c3d_bounded_phase_rejection_v1',numerically_accepted=False,physical_accuracy_certified=False,
            numerical_failure_reasons=['resources'],resource_phase_rejected=stopped.record,
            iterations=0,final_residual=None)
        if a.snapshot is not None:
            assert not a.snapshot.exists()
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({'numerically_accepted':row['numerically_accepted'],
        'iterations':row['iterations'],'final_residual':row['final_residual']}),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
