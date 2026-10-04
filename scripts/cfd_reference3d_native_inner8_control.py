"""One paired original-cube action benchmark; no flow qualification."""
import argparse,json,resource,time,weakref
from pathlib import Path
import numpy as np
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_distributed_p3_condensed import DistributedP3CondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_preconditioner import array_sha,matrix_sha
from cfd_reference3d_native_inner8_pressure import NativeInner8PressureFactor,fresh_admission,work_reserve
from cfd_reference3d_p3_cg8_scalar import BalancedSparse,BlockIC0,inner_cg8
from cfd_reference3d_pressure_complement10 import polynomial_basis,reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped

def control_reserve(nv,nc):return int(8*(128*nv+8*nc)+16*2**20)

def run(factor_library,coarse_library,snapshot):
    start=time.monotonic();admissions=[];f=None
    def sample(phase):
        a=dict(phase=phase,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        print(json.dumps(a),flush=True);enforce_phase(phase,a['peak_rss_bytes'],a['wall_s'])
    mesh,lo,hi,axes,macros=domain_mesh(4.,2,False,3,1,'original')
    s=DistributedP3CondensedSystem(mesh,.1,fixed_boundaries=('walls','body'),stage_callback=sample)
    nv=len(s.retained_free)-s.nmacro;C=BlockTriangle(s.upper_matrix,nv)
    identity=dict(mesh_sha256=array_sha(mesh.p,mesh.t),stored_block_triangle_sha256={name:matrix_sha(m) for name,m in (('velocity',C.velocity.upper),('coupling',C.coupling),('pressure',C.pressure.upper))})
    C.velocity=VectorTriangle(C.velocity.upper,factor_library);identity['vector_storage_sha256']=C.velocity.input_sha256
    outer=basis_reservation(C.shape[0],30);pw=reserve(nv,len(s.volumes));cw=control_reserve(nv,s.p3.Z.shape[1]);total=outer+pw+work_reserve(nv,s.p3.Z.shape[1])+cw
    def guarded(phase):
        sample(phase)
        if phase=='workspace_pressure_complete':
            a=fresh_admission(f,outer+cw,pw);assert a['basis_reservation_bytes']==total;admissions.append(a);print(json.dumps(dict(phase='numeric_stage_admission',**a)),flush=True)
            if not a['numeric_stage_admitted']:raise PhaseResourceStopped(dict(phase='numeric_stage_admission',**a,rss_cap_bytes=1800*2**20,wall_cap_s=180))
    try:
        f=NativeInner8PressureFactor.__new__(NativeInner8PressureFactor)
        f.__init__(C.velocity,factor_library,coarse_library=coarse_library,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata,stage_callback=guarded,live_arrays=(C.coupling.data,C.pressure.upper.data,s.volumes),pattern_reservation_bytes=total)
        baseline=BalancedSparse(C.velocity,f.Z,f.coarse_factor,lambda x:inner_cg8(lambda u:C.velocity@u,lambda r:BlockIC0.solve(f,r),x))
        centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T;Z=polynomial_basis(centers,s.volumes,.1,4.)
        random=np.random.default_rng(2221).normal(size=(nv,10));random/=np.linalg.norm(random,axis=0)
        loads=np.column_stack((C.coupling@Z,random));del random
        sample('benchmark_inputs_ready');before=storage_sha(loads);timings={'baseline':[],'native':[]};errors=[];positive=[];saved={}
        for batch in range(3):
            order=(('baseline',baseline.solve),('native',f.solve)) if batch%2==0 else (('native',f.solve),('baseline',baseline.solve))
            for name,action in order:
                begin=time.monotonic();saved[name]=[action(rhs) for rhs in loads.T];timings[name].append(time.monotonic()-begin)
            for j in range(20):
                old=saved['baseline'][j];new=saved['native'][j]
                errors.append(float(np.linalg.norm(new-old)/max(np.linalg.norm(old),1e-30)));positive.append(float(loads[:,j]@new))
            sample('paired_batch_'+str(batch+1))
        ratio=float(np.median(timings['native'])/np.median(timings['baseline']));selection=dict(eligibility_gate_passed=max(errors)<=1e-10 and min(positive)>0 and ratio<=.95,time_ratio=ratio,time_ratio_threshold=.95,max_relative_action_difference=max(errors),action_difference_threshold=1e-10,min_positive_work=min(positive),scope='same-math action-cost eligibility; full original flow/runtime/resource/force still required')
        assert before==storage_sha(loads) and f.input_unchanged() and C.velocity.input_unchanged()
        baseline_outputs=np.column_stack(saved['baseline']);native_outputs=np.column_stack(saved['native']);del saved,order,action,old,new
        metadata=f.metadata;refs=[weakref.ref(v) for v in (f,f.balanced,f.linear_pressure,f.starts,f.rounded,f.coarse_factor,f.coarse_factor.starts,f.coarse_factor.values,baseline)]
        del baseline;f.close();del f;retired=all(r() is None for r in refs);assert retired;sample('benchmark_factors_retired')
        assert not snapshot.exists();np.savez(snapshot,load_vectors=loads,baseline_responses=baseline_outputs,native_responses=native_outputs);sample('benchmark_serialized')
        return dict(diagnostic_accepted=True,numerically_accepted=False,flow_field_published=False,physical_accuracy_certified=False,tetrahedra=mesh.nelements,identity=identity,selection=selection,paired_batch_seconds=timings,loads_sha256=before,load_count=20,batches=3,original_inputs_preserved=True,diagnostic_factor_owners_retired=retired,admissions=admissions,reserved_work_bytes=total,control_reservation_bytes=cw,factor=metadata,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
    finally:
        if 'f' in locals() and f is not None:f.close()
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--coarse-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.factor_library,a.coarse_library,a.snapshot)
    except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,flow_field_published=False,resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','selection','resource_phase_rejected','wall_s','peak_rss_bytes')}),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
