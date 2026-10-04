"""Diagnostic-only inclusive timing of the exact matched packed preconditioner."""
import argparse,gc,inspect,json,resource,time,weakref
from pathlib import Path
from unittest.mock import patch
import numpy as np
import cfd_reference3d_packed_matched_l4_probe as matched
from cfd_reference3d_domain_budget import PhaseResourceStopped,enforce_phase
from cfd_reference3d_shared_factor import storage_sha

def diagnostic_reserve(n):
    if not isinstance(n,int) or n<1:raise ValueError('positive diagnostic dimension required')
    return 8*8*n+2*2**20

class TimedOperator:
    def __init__(self,owner,record):self.owner=owner;self.shape=owner.shape;self.record=record
    def __matmul__(self,x):
        start=time.monotonic()
        try:return self.owner@x
        finally:self.record('outer_velocity_action',time.monotonic()-start)

def profile_actions(factor,precondition,rhs,nv,check=lambda phase:None):
    rhs=np.asarray(rhs,dtype=float)
    if rhs.ndim!=1 or not 0<nv<len(rhs) or not np.all(np.isfinite(rhs)):raise ValueError('invalid mixed profile input')
    n=len(rhs);counts={};seconds={};per_load=[];positive=[];hash_before=storage_sha(rhs)
    def record(name,elapsed):counts[name]=counts.get(name,0)+1;seconds[name]=seconds.get(name,0.)+elapsed
    def timed(name,fn):
        def run(x):
            start=time.monotonic()
            try:return fn(x)
            finally:record(name,time.monotonic()-start)
        return run
    balanced=factor.balanced;coarse=factor.coarse_factor
    inverse=balanced.inverse;velocity=balanced.velocity;solve=coarse.solve;base=coarse.base_solve;action=coarse.action
    original=precondition(rhs);original_sha=storage_sha(original)
    kinds=('original_mixed_rhs','momentum_0','momentum_1','momentum_2','pressure_0','pressure_1','pressure_2')
    rng=np.random.default_rng(245713)
    with patch.object(balanced,'inverse',timed('local_inner8',inverse)),patch.object(balanced,'velocity',TimedOperator(velocity,record)),patch.object(coarse,'solve',timed('coarse_correction',solve)),patch.object(coarse,'base_solve',timed('coarse_Float_solve',base)),patch.object(coarse,'action',timed('coarse_Double_action',action)):
        for kind in kinds:
            load=rhs.copy() if kind=='original_mixed_rhs' else np.zeros(n)
            if kind.startswith('momentum'):load[:nv]=rng.normal(size=nv)
            elif kind.startswith('pressure'):load[nv:]=rng.normal(size=n-nv)
            if kind!='original_mixed_rhs':load/=np.linalg.norm(load)
            before=storage_sha(load);elapsed=[];outputs=[]
            for repeat in range(2):
                start=time.monotonic();out=precondition(load);elapsed.append(time.monotonic()-start)
                if not np.all(np.isfinite(out)):raise ValueError('nonfinite profile action')
                outputs.append(storage_sha(out));positive.append(float(load[:nv]@out[:nv]) if kind.startswith('momentum') else None)
                if kind=='original_mixed_rhs' and storage_sha(out)!=original_sha:raise ValueError('instrumented original action changed')
                check('profile_'+kind+'_'+str(repeat))
            if before!=storage_sha(load) or outputs[0]!=outputs[1]:raise ValueError('profile input or deterministic action changed')
            per_load.append(dict(kind=kind,seconds=elapsed,input_sha256=before,output_sha256=outputs[0]))
    assert balanced.inverse is inverse and balanced.velocity is velocity and coarse.solve == solve and coarse.base_solve == base and coarse.action == action
    if hash_before!=storage_sha(rhs) or not factor.input_unchanged():raise ValueError('profile altered original factor or RHS')
    total=sum(sum(x['seconds']) for x in per_load);exclusive=total-seconds['local_inner8']-seconds['coarse_correction']-seconds['outer_velocity_action']
    return dict(load_seed=245713,load_count=7,repeats=2,per_load=per_load,inclusive_seconds=seconds,call_counts=counts,total_preconditioner_s=total,remaining_balanced_and_pressure_s=exclusive,momentum_positive_work=[x for x in positive if x is not None],original_rhs_sha256=hash_before,instrumentation_preserves_original_action=True,instrumentation_restored=True,scope='inclusive single-thread mixed-action sample; synthetic load mixture is not a stalled-vector or full-spectrum certificate')

class ProfileComplete(Exception):
    def __init__(self,row,refs):self.row=row;self.refs=refs

def run(factor_library,coarse_library,snapshot):
    started=time.monotonic();original_basis=matched.probe.basis_reservation
    def checked(phase):enforce_phase(phase,resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-started)
    def intercept(C,rhs,precondition,target,maxiter,restart,metric,callback):
        if (target,maxiter,restart)!=(1e-11,3000,30):raise ValueError('original solver contract changed')
        closure=inspect.getclosurevars(precondition).nonlocals;factor=closure['velocity'].__self__;nv=factor.n
        refs=[weakref.ref(x) for x in (factor,factor.balanced,factor.linear_pressure,factor.coarse_factor,factor.starts,factor.rounded,factor.coarse_factor.starts,factor.coarse_factor.values)]
        try:
            report=profile_actions(factor,precondition,rhs,nv,checked)
            row=dict(diagnostic_accepted=True,numerically_accepted=False,flow_field_published=False,physical_accuracy_certified=False,tetrahedra=28416,profile=report,factor=factor.metadata,pressure_preconditioner=closure['coarse'].metadata,diagnostic_reservation_bytes=diagnostic_reserve(len(rhs)),original_target=1e-10,retained_target=target,outer_restart=restart,outer_iteration_cap=maxiter,owned_peak_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        finally:factor.close()
        raise ProfileComplete(row,refs)
    result=None;refs=None
    try:
        with patch.object(matched.probe,'flexible_gmres',intercept),patch.object(matched.probe,'basis_reservation',lambda n,r:original_basis(n,r)+diagnostic_reserve(n)):
            matched.run(factor_library=factor_library,coarse_library=coarse_library,snapshot=snapshot)
    except ProfileComplete as e:
        result=e.row;refs=e.refs;e.__traceback__=None
    if result is None:raise ValueError('profile did not intercept original full solve')
    gc.collect();result['diagnostic_factor_owners_retired']=all(x() is None for x in refs)
    if not result['diagnostic_factor_owners_retired']:raise ValueError('profile factor owner retained')
    assert not snapshot.exists();np.savez(snapshot,per_load_seconds=np.array([x['seconds'] for x in result['profile']['per_load']]))
    checked('profile_serialized');result['wall_s']=time.monotonic()-started
    return result
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--coarse-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.factor_library,a.coarse_library,a.snapshot)
    except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,flow_field_published=False,resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','resource_phase_rejected','wall_s','diagnostic_factor_owners_retired')}),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
