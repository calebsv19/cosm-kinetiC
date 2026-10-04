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
from cfd_reference3d_residency_owners import fresh_admission
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_requested_target import requested_full_linear_acceptance
from cfd_reference3d_retained_margin import validate_margin
from cfd_reference3d_pressure_coarse import build as build_pressure_coarse,reserve as coarse_reserve
from cfd_reference3d_pressure_coverage import analyze,reserve as diagnostic_reserve
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
            extra=diagnostic_reserve(nv,len(system.volumes))
            admission=fresh_admission(exact_factor,basis_reservation(C.shape[0],restart),coarse_bytes+extra)
            admission.update(pressure_diagnostic_reservation_bytes=extra)
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
    SelectedFactor=VectorWorkspaceCholesky if storage_selection['encoded_selected'] else LegacyFactor
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
    SelectedFactor.__init__(exact_factor,C.velocity,factor_library,factor_order,live_arrays=live_arrays,action=lambda:C@control_vector,stage_callback=sample)
    velocity=exact_factor.solve
    meta=dict(exact_factor.metadata,operator_representation='complete symmetric velocity/pressure triangles plus both coupling actions; all mixed entries retained')
    coarse=None
    pressure_meta={'kind':'negative_exact_macro_constant_mass_in_upper_triangular_preconditioner'}
    if coarse_pressure=='quadratic':
        coarse=build_pressure_coarse(mesh,system.volumes,mu,length,C.coupling,C.pressure,velocity)
        pressure_meta=coarse.metadata.copy()
        sample('coarse_pressure_ready')
    sample('diagnostic_start')
    centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T
    diagnostic=analyze(centers,system.volumes,mu,length,lo,hi,C.coupling,C.pressure,velocity,coarse,mesh,sample)
    if not exact_factor.input_unchanged() or not C.velocity.input_unchanged() or storage_sha(*other_arrays)!=other_hash:raise ValueError('diagnostic altered original inputs')
    sample('pressure_diagnostic_complete')
    exact_factor.close();del velocity,exact_factor,coarse
    sample('factor_released')
    return dict(schema='physics_sim_c3d_pressure_sample_diagnostic_v1',diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,flow_field_published=False,identity=identity,diagnostic=diagnostic,preconditioner=meta,pressure_preconditioner=pressure_meta,resource_samples=resource_samples,tetrahedra=mesh.nelements,condensed_free_dofs=C.shape[0],pressure_dofs=len(system.volumes),peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-started)
