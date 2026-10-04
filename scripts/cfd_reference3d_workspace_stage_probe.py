#!/usr/bin/env python3
"""Bounded exact-condensed cube solve; original FE quadrature verifies full fields."""
import argparse
import json
import time
import resource
from unittest.mock import patch
import cfd_reference3d_workspace_cholesky as workspace_module
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import minres,gmres,LinearOperator,splu
from skfem import FacetBasis,LinearForm,asm
from cfd_reference3d_shared_factor import BlockTriangle
from cfd_reference3d_workspace_cholesky import WorkspaceCholesky
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


def run(length=4.,count=2,split=False,exponent=3,outer_layers=1,target=1e-10,maxiter=3000,kind='workspace_coupled_cholesky',snapshot=None,chunk_size=128,drop_tol=1e-4,fill_factor=8.,ssor_cycles=1,factor_library=None,factor_order='metis',cache_mib=0,domain_mesh_mode='original'):
    if kind!='workspace_coupled_cholesky':raise ValueError('triangle probe requires exact coupled Cholesky')
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
    system.metadata['block_storage']=dict(stored_nnz=C.stored_nnz,stored_array_bytes=C.stored_bytes,scope='complete exact velocity/coupling/pressure partition; no coefficients dropped')
    assembly=time.monotonic()-started
    print(json.dumps({'phase':'assembled',**system.metadata,'identity':identity,'assembly_s':assembly,
        'peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    sample('assembly_complete')
    live_arrays=tuple(a for matrix in (C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,full_rhs,system.volumes,retained_free)
    control_vector=np.random.default_rng(1211).normal(size=C.shape[0]);captured={}
    original_arrays=tuple(a for matrix in (C.velocity.upper,C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,full_rhs,system.volumes,retained_free)
    from cfd_reference3d_shared_factor import storage_sha
    digest=storage_sha(*original_arrays)
    factor=WorkspaceCholesky.__new__(WorkspaceCholesky)
    pressure_function=workspace_module.release_free_pages
    def record_pressure(*args,**kwargs):
        row=pressure_function(*args,**kwargs);captured['pressure']=row;return row
    class StageCaptured(Exception):pass
    def stage(phase):
        sample(phase)
        if phase=='workspace_pressure_complete':
            storage=int(factor.library.cfd_reference_factor_storage(factor._handle))
            scratch=int(factor.library.cfd_reference_factor_numeric_workspace(factor._handle))
            resident=captured['pressure']['current_rss_after_bytes'];reserve=32*1024**2
            captured.update(factor_storage_bytes=storage,numeric_workspace_bytes=scratch,current_rss_before_numeric_bytes=resident,
                reserve_bytes=reserve,estimated_numeric_stage_bytes=resident+storage+scratch+reserve,numeric_stage_admitted=resident+storage+scratch+reserve<=1800*1024**2)
            raise StageCaptured()
    try:
        with patch('cfd_reference3d_workspace_cholesky.release_free_pages',side_effect=record_pressure):
            WorkspaceCholesky.__init__(factor,C.velocity.upper,factor_library,factor_order,live_arrays=live_arrays,action=lambda:C@control_vector,stage_callback=stage)
        raise AssertionError('stage diagnostic attempted numeric factor')
    except StageCaptured:pass
    assert factor._handle is None and storage_sha(*original_arrays)==digest
    return dict(schema='physics_sim_c3d_workspace_stage_probe_v1',diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,
        numerical_field_published=False,numeric_factor_attempted=False,symbolic_handle_cleanup_verified=True,original_mixed_input_preserved=True,
        original_mixed_input_sha256=digest,identity=identity,admission=captured,length=length,count=count,split_first_normal=split,
        tetrahedra=mesh.nelements,condensation=system.metadata,resource_samples=resource_samples,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        wall_s=time.monotonic()-started,scope='exact symbolic/current-residency stage admission estimate; no numeric inverse/full FE acceptance/field/force result')


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=6)
    ap.add_argument('--split',action='store_true');ap.add_argument('--factor-library',type=Path,required=True)
    ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.length,a.count,a.split,factor_library=a.factor_library)
    except PhaseResourceStopped as error:
        row=dict(schema='physics_sim_c3d_workspace_stage_probe_v1',diagnostic_accepted=False,numerically_accepted=False,physical_accuracy_certified=False,numerical_field_published=False,numeric_factor_attempted=False,resource_phase_rejected=error.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');assert not a.snapshot.exists()
    print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','admission','wall_s','peak_rss_bytes','resource_phase_rejected')}),flush=True)
    raise SystemExit(0 if row['diagnostic_accepted'] else 2)
