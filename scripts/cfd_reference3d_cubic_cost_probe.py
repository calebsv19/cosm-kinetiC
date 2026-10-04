#!/usr/bin/env python3
"""Bounded exact cube graph/cost diagnostic; never publishes a numerical field."""
import argparse,json,time,resource
from pathlib import Path
import numpy as np
from skfem import FacetBasis,LinearForm,asm
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_symbolic import SymbolicFactor
from cfd_reference3d_cubic_coarse import macro_cubic_interpolation
from scipy.sparse import triu
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem as CondensedSystem
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped
from cfd_reference3d_preconditioner import array_sha,matrix_sha


def run(length=4.,count=6,split=False,exponent=3,outer_layers=1,cache_mib=0,domain_mesh_mode='original',graph_mode='component_major',symbolic_library=None,symbolic_order='metis'):
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
    original_arrays=tuple(a for matrix in (C.velocity.upper,C.coupling,C.pressure.upper) for a in (matrix.indptr,matrix.indices,matrix.data))+(rhs,full_rhs,system.volumes,retained_free)
    original_digest=storage_sha(*original_arrays)
    begin=time.monotonic();Z,coarse_meta=macro_cubic_interpolation(system,nv)
    A=C.velocity.upper;AZ=A@Z+A.T@Z-Z.multiply(A.diagonal()[:,None]);Ac=(Z.T@AZ).tocsr()
    asymmetry=Ac-Ac.T;relative=float(np.linalg.norm(asymmetry.data)/max(np.linalg.norm(Ac.data),1e-30))
    if relative>1e-10:raise ValueError('coarse Galerkin asymmetry')
    coarse=triu((Ac+Ac.T)*.5,format='csr');coarse.sort_indices();del AZ,Ac,asymmetry
    bounds=(0,nf,nv)
    graphs={'coarse':coarse,'longitudinal':A[:nf,:nf],'transverse':A[nf:,nf:]}
    graph_s=time.monotonic()-begin;sample('graphs_ready')
    costs={};begin=time.monotonic()
    for name,graph in graphs.items():
        symbolic=SymbolicFactor(graph,symbolic_library,symbolic_order,1)
        assert symbolic.input_unchanged();costs[name]=dict(symbolic.metadata,graph_sha256=matrix_sha(graph))
        sample('symbolic_'+name);symbolic.close();del symbolic
    symbolic_s=time.monotonic()-begin;sample('symbolic_released')
    assert storage_sha(*original_arrays)==original_digest
    metadata=dict(blocks=costs,total_factor_storage_bytes=sum(c['factor_storage_bytes'] for c in costs.values()),
        maximum_numeric_workspace_bytes=max(c['numeric_workspace_bytes'] for c in costs.values()),coarse_galerkin_relative_asymmetry=relative,
        requirement_scope='exact coarse plus principal two-block factor storage; excludes physical/interpolation/input/FE/symbolic/runtime allocations; not RSS prediction')
    return dict(schema='physics_sim_c3d_cubic_cost_probe_v1',diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,
        numerical_field_published=False,identity=identity,coarse_interpolation=coarse_meta,
        original_mixed_input_preserved=True,original_mixed_input_sha256=original_digest,symbolic=metadata,
        length=length,count=count,split_first_normal=split,domain_mesh_mode=domain_mesh_mode,axis_nodes_m=[a.tolist() for a in axes],
        tetrahedra=mesh.nelements,velocity_degree=4,pressure_degree=3,full_velocity_dofs=int(3*ub.N),full_pressure_dofs=int(pb.N),
        condensed_free_dofs=C.shape[0],velocity_prefix_dofs=nv,macro_tetrahedra=macros,condensation=system.metadata,
        timings=dict(assembly_s=assembly,graph_construction_s=graph_s,symbolic_s=symbolic_s),resource_samples=resource_samples,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,wall_s=time.monotonic()-started,
        scope='structural factor/workspace diagnostic only; no numerical inverse, SPD/full residual proof, accepted field or force result')


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--length',type=float,choices=(4.,8.),default=4.);ap.add_argument('--count',type=int,choices=(2,4,6),default=6)
    ap.add_argument('--split',action='store_true');ap.add_argument('--graph-mode',choices=('component_major','interleaved_scalar','vector_block'),default='component_major')
    ap.add_argument('--symbolic-order',choices=('amd','metis'),default='metis');ap.add_argument('--symbolic-library',type=Path,required=True)
    ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.length,a.count,a.split,graph_mode=a.graph_mode,symbolic_library=a.symbolic_library,symbolic_order=a.symbolic_order)
    except PhaseResourceStopped as error:
        row=dict(schema='physics_sim_c3d_cubic_cost_probe_v1',diagnostic_accepted=False,numerically_accepted=False,physical_accuracy_certified=False,numerical_field_published=False,resource_phase_rejected=error.record)
    a.output.write_text(json.dumps(row,indent=2)+chr(10));assert not a.snapshot.exists()
    print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','symbolic','wall_s','peak_rss_bytes','resource_phase_rejected')}),flush=True)
    raise SystemExit(0 if row['diagnostic_accepted'] else 2)
