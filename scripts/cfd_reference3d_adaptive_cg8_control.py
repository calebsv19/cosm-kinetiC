"""One bounded original-cube pressure-column diagnostic; no flow field."""
import argparse,json,time,resource,weakref
from pathlib import Path
import numpy as np
from skfem import FacetBasis,LinearForm,asm
from cfd_reference3d_pressure_column_proxy import stage_admission,columns
from cfd_reference3d_energy_cg8 import diagnostic_reserve
from cfd_reference3d_distributed_p3_condensed import DistributedP3CondensedSystem
from cfd_reference3d_adaptive_cg8_pressure import AdaptiveCG8PressureFactor as DistributedP3CG8ScalarPressureFactor,fresh_admission,work_reserve
from cfd_reference3d_p3_cg8_scalar import BalancedSparse,BlockIC0,inner_cg8
from cfd_reference3d_p3_cg16_scalar import inner_cg16
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky
from cfd_reference3d_distributed_p2 import ExactCoarse
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import polynomial_basis,BalancedPressure,reserve
from cfd_reference3d_pressure_coverage import basis,projected
from cfd_reference3d_preconditioner import array_sha,matrix_sha

def run(factor_library,coarse_library,double_library,snapshot):
 start=time.monotonic();admissions=[];p3=None;f_float=None;f_double=None
 def sample(phase):
  record=dict(phase=phase,wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss);print(json.dumps(record),flush=True);enforce_phase(phase,record['peak_rss_bytes'],record['wall_s'])
 def admit(name,storage,scratch):
  a=stage_admission(int(storage),int(scratch),work,int(current_rss_bytes()));a.update(phase=name,outer_basis_reservation_bytes=outer,pressure_reservation_bytes=pressure_work,distributed_work_reservation_bytes=distributed_work,diagnostic_reservation_bytes=diagnostic_work);admissions.append(a);print(json.dumps(a),flush=True)
  if not a['numeric_stage_admitted']:raise PhaseResourceStopped(dict(a,rss_cap_bytes=1800*2**20,wall_cap_s=180))
  sample(name)
 mesh,lo,hi,axes,macros=domain_mesh(4.,2,False,3,1,'original');print(json.dumps(dict(phase='mesh',tetrahedra=mesh.nelements)),flush=True)
 s=DistributedP3CondensedSystem(mesh,.1,fixed_boundaries=('walls','body'),stage_callback=sample);nv=len(s.retained_free)-s.nmacro;C=BlockTriangle(s.upper_matrix,nv);A=C.velocity;physical_upper=A.upper
 identity=dict(mesh_sha256=array_sha(mesh.p,mesh.t),stored_block_triangle_sha256={name:matrix_sha(m) for name,m in (('velocity',A.upper),('coupling',C.coupling),('pressure',C.pressure.upper))})
 fixed=s.ub.get_dofs(['walls','body']).all();fullfree=np.setdiff1d(np.arange(s.ub.N),fixed)
 @LinearForm
 def inlet(v,w):return v
 f=asm(inlet,FacetBasis(mesh,s.ub.elem,facets=mesh.boundaries['inlet'],intorder=8));rhs=np.r_[f[fullfree],np.zeros(2*len(fullfree)+s.pb.N)];identity.update(free_dofs_sha256=array_sha(fullfree),rhs_sha256=array_sha(rhs));del rhs,f,fullfree,fixed
 C.velocity=VectorTriangle(A.upper,factor_library);identity.update(vector_storage_sha256=C.velocity.input_sha256);check=np.random.default_rng(1211).normal(size=nv);change=float(np.linalg.norm(C.velocity@check-A@check)/np.linalg.norm(A@check));assert change<=1e-12
 before=storage_sha(*(a for m in (physical_upper,C.coupling,C.pressure.upper,s.p3.Z,s.p3.upper) for a in (m.indptr,m.indices,m.data)))
 outer=basis_reservation(C.shape[0],30);pressure_work=reserve(nv,len(s.volumes));distributed_work=work_reserve(nv,s.p3.Z.shape[1]);diagnostic_work=diagnostic_reserve(nv,len(s.volumes));work=outer+pressure_work+distributed_work+diagnostic_work
 def p3_sample(phase):
  sample(phase)
  if phase=='workspace_pressure_complete':
   a=fresh_admission(p3,outer+diagnostic_work,pressure_work);assert a['basis_reservation_bytes']==work;admissions.append(dict(phase='p3_numeric_admission',**a));print(json.dumps(admissions[-1]),flush=True)
   if not a['numeric_stage_admitted']:raise PhaseResourceStopped(dict(phase='p3_numeric_admission',**a,rss_cap_bytes=1800*2**20,wall_cap_s=180))
 try:
  p3=DistributedP3CG8ScalarPressureFactor.__new__(DistributedP3CG8ScalarPressureFactor);p3.__init__(C.velocity,factor_library,coarse_library=coarse_library,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata,stage_callback=p3_sample,live_arrays=(C.coupling.indptr,C.coupling.indices,C.coupling.data,C.pressure.upper.indptr,C.pressure.upper.indices,C.pressure.upper.data,s.volumes))
  cg16=BalancedSparse(C.velocity,p3.Z,p3.coarse_factor,lambda x:inner_cg16(lambda u:C.velocity@u,lambda r:BlockIC0.solve(p3,r),x))
  def float_sample(phase):
   sample('complete_float_'+phase)
   if phase=='workspace_pressure_complete':admit('float_numeric_admission',f_float.library.cfd_reference_factor_storage(f_float._handle),f_float.library.cfd_reference_factor_numeric_workspace(f_float._handle))
  f_float=MixedWorkspaceCholesky.__new__(MixedWorkspaceCholesky);f_float.__init__(C.velocity,factor_library,stage_callback=float_sample,live_arrays=(s.volumes,C.coupling.data,C.pressure.upper.data))
  f_double=ExactCoarse(physical_upper,double_library,numeric=False);assert f_double.input_unchanged();admit('double_numeric_admission',f_double.storage,f_double.numeric_workspace);f_double.numeric();sample('all_factors_ready')
  centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T;Z=polynomial_basis(centers,s.volumes,.1,4.);loads=C.coupling@Z
  baseline_counts=dict(factor_applications=0,velocity_applications=0)
  def old_G(x):baseline_counts['factor_applications']+=1;return BlockIC0.solve(p3,x)
  def old_A(x):baseline_counts['velocity_applications']+=1;return C.velocity@x
  baseline_balance=BalancedSparse(C.velocity,p3.Z,p3.coarse_factor,lambda x:inner_cg8(old_A,old_G,x));old_before=baseline_counts.copy();new_before={k:p3.inner_statistics[k] for k in ('calls','steps','factor_applications','velocity_applications','early_returns')};adaptive_traces=[]
  actions={'fixed':p3.pressure_solve,'cg8':baseline_balance.solve,'adaptive':p3.solve,'cg16':cg16.solve,'qualified_float':f_float.solve,'exact_double':f_double.solve};responses={};W={};times={}
  for name,action in actions.items():
   begin=time.monotonic();cols=[]
   for rhs in loads.T:
    cols.append(action(rhs))
    if name=='adaptive':adaptive_traces.append([v.copy() for v in p3.inner_statistics['last_trace']])
   responses[name]=np.column_stack(cols);del cols;W[name]=C.coupling.T@responses[name]-C.pressure@Z;times[name]=time.monotonic()-begin;sample('pressure_columns_'+name)
  metrics={name:columns(lambda x:C.velocity@x,loads,u,Z,W[name],responses['exact_double'],W['exact_double']) for name,u in responses.items()};assert max(metrics['exact_double']['velocity_true_relative_residuals'])<=1e-10 and min(metrics['exact_double']['velocity_positive_work'])>0
  Q,coverage_meta=basis(centers,s.volumes,.1,4.,lo,hi);root=np.sqrt(s.volumes/.1);Y={}
  for name in ('qualified_float','exact_double'):
   Y[name]=np.empty_like(Q)
   for j in range(Q.shape[1]):
    pp=Q[:,j]/root;Y[name][:,j]=(C.coupling.T@actions[name](C.coupling@pp)-C.pressure@pp)/root
    if (j+1)%10==0:sample('coverage_'+name+'_'+str(j+1))
  coverage={};conditions={};pc_metadata={}
  for name in ('fixed','qualified_float'):
   coverage[name]={};conditions[name]={};pc_metadata[name]={}
   for scale in (10,):
    pc=BalancedPressure(Z,W[name],scale*.1/s.volumes);coverage[name][str(scale)]=projected(Q,Y['exact_double'],root,pc,centers,lo,hi,4.);conditions[name][scale]=coverage[name][str(scale)]['balanced_ten_sampled_condition'];pc_metadata[name][str(scale)]=pc.metadata
   del pc
  old_delta={k:baseline_counts[k]-old_before[k] for k in baseline_counts};new_delta={k:p3.inner_statistics[k]-new_before[k] for k in new_before};energy={}
  for name in ('cg8','adaptive'):
   delta=responses[name]-responses['exact_double'];energy[name]=[float(np.sqrt(max(0.,v@(C.velocity@v)))) for v in delta.T]
  energy_ratios=[a/max(b,1e-30) for a,b in zip(energy['adaptive'],energy['cg8'])];selection=dict(eligibility_gate_passed=new_delta['factor_applications']<=.75*old_delta['factor_applications'] and new_delta['velocity_applications']<=old_delta['velocity_applications'] and max(energy_ratios)<=1.25,baseline_work=old_delta,adaptive_work=new_delta,energy_error_ratios=energy_ratios,factor_work_ratio=new_delta['factor_applications']/old_delta['factor_applications'],scope='paired adaptive-PC cost/energy proxy only; original full convergence/runtime/force still required');mapping=mesh.mapping().A.transpose(2,0,1);jacobian=np.linalg.cond(mapping);cell_centers=mesh.p[:,mesh.t].mean(axis=1).T;near=np.linalg.norm(np.maximum(np.maximum(lo-cell_centers,cell_centers-hi),0),axis=1)<.1
  geometry=dict(Jacobian_condition_max=float(jacobian.max()),Jacobian_condition_median=float(np.median(jacobian)),near_body_condition_max=float(jacobian[near].max()),near_body_condition_median=float(np.median(jacobian[near])))
  assert p3.input_unchanged() and f_float.input_unchanged() and f_double.input_unchanged();assert before==storage_sha(*(a for m in (physical_upper,C.coupling,C.pressure.upper,s.p3.Z,s.p3.upper) for a in (m.indptr,m.indices,m.data)))
  metadata=dict(p3=p3.metadata,qualified_float=f_float.metadata,exact_double=f_double.metadata);refs=[weakref.ref(a) for a in (p3.starts,p3.rows,p3.perm,p3.inverse_permutation,p3.rounded,p3.coarse_factor.starts,p3.coarse_factor.values,f_float.rounded,f_double.starts)];owner_refs=[weakref.ref(a) for a in (p3,f_float,f_double,cg16)]
  del actions,action,cg16,baseline_balance;p3.close();f_float.close();f_double.close();del p3,f_float,f_double;retired=all(r() is None for r in refs+owner_refs);assert retired;sample('diagnostic_factors_retired')
  row=dict(diagnostic_accepted=True,numerically_accepted=False,physical_accuracy_certified=False,complete_spectrum_certified=False,flow_field_published=False,tetrahedra=mesh.nelements,identity=identity,conversion_full_mixed_relative_action_change=change,original_inputs_preserved=True,pressure_constant_kept=True,diagnostic_factor_owners_retired=retired,admissions=admissions,adaptive_proxy_traces=adaptive_traces,columns=metrics,column_wall_s=times,coverage_basis=coverage_meta,coverage=coverage,pressure_preconditioners=pc_metadata,selection=selection,geometry=geometry,factors=metadata,reserved_work_bytes=work,scope='same original small-cube velocity/pressure diagnostics only; original full physical equations and field qualification not replaced')
  sample('diagnostic_ready');assert not snapshot.exists();np.savez(snapshot,pressure_basis=Z,coverage_basis_mass=Q,qualified_mass_schur_columns=Y['qualified_float'],exact_mass_schur_columns=Y['exact_double'],**{'pressure_schur_'+k:v for k,v in W.items()},**{'velocity_load_response_'+k:v for k,v in responses.items()});sample('diagnostic_serialized');row.update(wall_s=time.monotonic()-start,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss);return row
 finally:
  for obj in (p3 if 'p3' in locals() else None,f_float if 'f_float' in locals() else None,f_double if 'f_double' in locals() else None):
   if obj is not None:obj.close()
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--coarse-library',type=Path,required=True);ap.add_argument('--double-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
 try:row=run(a.factor_library,a.coarse_library,a.double_library,a.snapshot)
 except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,flow_field_published=False,resource_phase_rejected=e.record)
 a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','selection','resource_phase_rejected','wall_s','peak_rss_bytes')}),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
