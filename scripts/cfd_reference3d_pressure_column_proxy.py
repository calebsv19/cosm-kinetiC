"""Independent pressure-column metrics and bounded sampled scale eligibility."""
import numpy as np

def diagnostic_reserve(nv,np_):
    if not isinstance(nv,int) or not isinstance(np_,int) or nv<1 or np_<10:raise ValueError('invalid diagnostic reservation')
    return int(8*(5*nv*10+12*nv+16*np_*64+12*64**2)+8*2**20)

def stage_admission(storage,scratch,work,rss):
    if any(not isinstance(x,int) or x<0 for x in (storage,scratch,work)) or not isinstance(rss,int) or rss<=0:raise ValueError('invalid fresh admission')
    a=dict(current_rss_before_numeric_bytes=rss,factor_storage_bytes=storage,numeric_workspace_bytes=scratch,basis_and_diagnostic_reservation_bytes=work,reserve_bytes=32*2**20)
    a['estimated_numeric_stage_bytes']=sum(a.values());a['numeric_stage_admitted']=a['estimated_numeric_stage_bytes']<=1800*2**20;return a

def columns(action,rhs,solutions,Z,W,reference,reference_W):
    if rhs.shape!=solutions.shape or solutions.shape!=reference.shape or Z.shape!=W.shape or W.shape!=reference_W.shape or rhs.shape[1]!=Z.shape[1] or not all(np.all(np.isfinite(a)) for a in (rhs,solutions,Z,W,reference,reference_W)):raise ValueError('invalid diagnostic columns')
    residual=[];solution=[];energy=[]
    for j in range(rhs.shape[1]):
        den=max(float(np.linalg.norm(rhs[:,j])),1e-30);residual.append(float(np.linalg.norm(action(solutions[:,j])-rhs[:,j])/den));solution.append(float(np.linalg.norm(solutions[:,j]-reference[:,j])/max(np.linalg.norm(reference[:,j]),1e-30)));energy.append(float(rhs[:,j]@solutions[:,j]))
    K=Z.T@W;Kr=Z.T@reference_W;skew=float(np.linalg.norm(K-K.T)/max(np.linalg.norm(K),1e-30))
    return dict(velocity_true_relative_residuals=residual,velocity_solution_relative_errors=solution,velocity_positive_work=energy,schur_column_relative_error=float(np.linalg.norm(W-reference_W)/max(np.linalg.norm(reference_W),1e-30)),projected_coarse_relative_error=float(np.linalg.norm(K-Kr)/max(np.linalg.norm(Kr),1e-30)),coarse_relative_skew=skew,diagonal_energy_relative_errors=[float(abs(K[j,j]/Kr[j,j]-1)) for j in range(len(K))],constant_pressure_direction_energy=float(K[0,0]),nonlinear_symmetry_gate_passed=skew<1e-5,scope='original velocity residual and projected pressure diagnostics; no field, full spectrum or nonlinear SPD assertion')

def select_scale(records):
    if set(records)!={'fixed','qualified_float'} or any(set(v)!={5,10,20,30} for v in records.values()):raise ValueError('declared scales/proxies required')
    base={k:float(v[10]) for k,v in records.items()}
    if not all(np.isfinite(x) and x>0 for v in records.values() for x in v.values()):raise ValueError('positive finite sampled conditions required')
    ratios={scale:max(records[k][scale]/base[k] for k in records) for scale in (5,10,20,30)};eligible=[s for s in (5,20,30) if ratios[s]<=.8];chosen=min(eligible,key=lambda s:(ratios[s],s)) if eligible else None
    return dict(selected_scale=chosen,worst_relative_sampled_condition={str(s):ratios[s] for s in ratios},eligibility_gate_passed=chosen is not None,scope='bounded sampled-subspace eligibility only; full original solve/runtime/force authority still required')
