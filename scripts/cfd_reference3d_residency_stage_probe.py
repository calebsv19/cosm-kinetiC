#!/usr/bin/env python3
"""Bounded exact-condensed cube solve; original FE quadrature verifies full fields."""
import argparse
import json
import time
import resource
import gc
from unittest.mock import patch
import cfd_reference3d_mixed_workspace as workspace_module
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import minres,gmres,LinearOperator,splu
from skfem import FacetBasis,LinearForm,asm
from cfd_reference3d_shared_factor import BlockTriangle
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky as VectorWorkspaceCholesky,numeric_stage_admission
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_residency import array_owners,fresh_admission
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_pressure_coarse import reserve as coarse_reserve
from cfd_reference3d_vector_storage import VectorTriangle
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
from cfd_reference3d_preconditioner import array_sha,matrix_sha
from cfd_reference3d_chunked import volume_metrics
from cfd_reference3d_traction import traction
from cfd_reference3d_graded_probe import numerical_failure_reasons,publish_snapshot


def run(length=4.,count=2,split=False,exponent=3,outer_layers=1,target=1e-10,maxiter=3000,kind='mixed_workspace_coupled_cholesky',snapshot=None,chunk_size=128,drop_tol=1e-4,fill_factor=8.,ssor_cycles=1,factor_library=None,factor_order='metis',cache_mib=0,domain_mesh_mode='original',restart=6):
    if restart != 6:raise ValueError('unsupported declared restart')
    if kind!='mixed_workspace_coupled_cholesky':raise ValueError('triangle probe requires exact coupled Cholesky')
    started=time.monotonic();mu=.1;Q=.008;resource_samples={}
    def sample(phase):
        resource_samples[phase]=dict(wall_s=time.monotonic()-started,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        print(json.dumps(dict(phase=phase,**resource_samples[phase])),flush=True)
        # Atomic publication owns its post-serialization acceptance check.
        if phase!='publication_complete':enforce_phase(phase,resource_samples[phase]['peak_rss_bytes'],resource_samples[phase]['wall_s'])
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
    C.stored_bytes=C.velocity.stored_bytes+sum(a.nbytes for a in other_arrays)
    identity.update(vector_storage_sha256=C.velocity.input_sha256,conversion_full_mixed_relative_action_change=action_change,pressure_coupling_bitwise_preserved=True,
        matrix_identity_scope='original complete scalar block CSR hashes before exact bitwise vector conversion; full mixed action and original FE authority retained')
    system.metadata['block_storage']=dict(original_mixed_scalar_upper_nnz=original_mixed_block_nnz,stored_array_bytes=C.stored_bytes,
        stored_dense_block_value_count=len(C.velocity.values),velocity=C.velocity.metadata,scope='complete node-block velocity plus unchanged coupling/pressure; no coefficients dropped')
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
    live_arrays=tuple(a for matrix in (C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,load.indices,load.values,system.volumes,retained_free)
    control_vector=np.random.default_rng(1211).normal(size=C.shape[0]);captured={}
    original_arrays=(C.velocity.starts,C.velocity.rows,C.velocity.values)+tuple(a for matrix in (C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,load.indices,load.values,system.volumes,retained_free)
    digest=storage_sha(*original_arrays)
    factor=VectorWorkspaceCholesky.__new__(VectorWorkspaceCholesky)
    pressure_function=workspace_module.release_free_pages
    def record_pressure(*args,**kwargs):
        row=pressure_function(*args,**kwargs)
        after_validation_rss=current_rss_bytes();inventory_begin=time.monotonic()
        inventory=array_owners({'physical_mixed':C,'factor_input':factor,'full_FE_and_mesh':system,'load_and_work':(load,rhs,control_vector,retained_free),'suspended_metadata':suspended})
        row.update(after_validation_rss_bytes=after_validation_rss,after_inventory_rss_bytes=current_rss_bytes(),owner_inventory=inventory,inventory_wall_s=time.monotonic()-inventory_begin,gc_enabled=gc.isenabled(),gc_counts=gc.get_count(),gc_stats=gc.get_stats(),explicit_collection_performed=False)
        captured['pressure']=row;return row
    class StageCaptured(Exception):pass
    def stage(phase):
        sample(phase)
        if phase=='workspace_pressure_complete':
            captured.update(fresh_admission(factor,basis_reservation(C.shape[0],restart),coarse_reserve(nv,len(system.volumes))))
            raise StageCaptured()
    try:
        with patch('cfd_reference3d_mixed_workspace.release_free_pages',side_effect=record_pressure):
            VectorWorkspaceCholesky.__init__(factor,C.velocity,factor_library,factor_order,live_arrays=live_arrays,action=lambda:C@control_vector,stage_callback=stage)
        raise AssertionError('stage diagnostic attempted numeric factor')
    except StageCaptured:pass
    assert factor._handle is None and factor.input_unchanged() and storage_sha(*original_arrays)==digest
    suspended.check_detached()
    return dict(schema='physics_sim_c3d_mixed_precision_stage_probe_v1',diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,
        numerical_field_published=False,numeric_factor_attempted=False,symbolic_handle_cleanup_verified=True,original_mixed_input_preserved=True,
        original_mixed_input_sha256=digest,identity=identity,admission=captured,outer_iteration={'restart':restart},length=length,count=count,split_first_normal=split,
        outer_layers=outer_layers,domain_mesh_mode=domain_mesh_mode,axis_nodes_m=[a.tolist() for a in axes],body_lo_m=lo.tolist(),body_hi_m=hi.tolist(),
        tetrahedra=mesh.nelements,condensation=system.metadata,resource_samples=resource_samples,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        wall_s=time.monotonic()-started,scope='exact symbolic/current-residency stage admission estimate; no numeric inverse/full FE acceptance/field/force result')


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--restart',type=int,choices=(6,),default=6)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=6)
    ap.add_argument('--domain-mesh-mode',choices=('original','held_l4'),default='original')
    ap.add_argument('--outer-layers',type=int,choices=(1,2,3),default=2)
    ap.add_argument('--chunk-size',type=int,choices=(128,512),default=128)
    ap.add_argument('--split',action='store_true');ap.add_argument('--factor-library',type=Path,required=True)
    ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.length,a.count,a.split,outer_layers=a.outer_layers,factor_library=a.factor_library,chunk_size=a.chunk_size,domain_mesh_mode=a.domain_mesh_mode,restart=a.restart)
    except PhaseResourceStopped as error:
        row=dict(schema='physics_sim_c3d_mixed_precision_stage_probe_v1',diagnostic_accepted=False,numerically_accepted=False,physical_accuracy_certified=False,numerical_field_published=False,numeric_factor_attempted=False,resource_phase_rejected=error.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');assert not a.snapshot.exists()
    print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','admission','wall_s','peak_rss_bytes','resource_phase_rejected')}),flush=True)
    raise SystemExit(0 if row['diagnostic_accepted'] else 2)
