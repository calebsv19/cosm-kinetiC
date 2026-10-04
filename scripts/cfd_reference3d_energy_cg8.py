"""Bounded adaptive PCG inner action; full original residual remains authority."""
import numpy as np
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve as old_diagnostic_reserve
ETA=.25

def diagnostic_reserve(nv,np_):return old_diagnostic_reserve(nv,np_)+16*nv*10

def new_statistics():return dict(calls=0,steps=0,factor_applications=0,velocity_applications=0,early_returns=0,step_histogram={str(j):0 for j in range(9)},last_trace=[])

def energy_cg8(action,precondition,x,stats=None):
 rhs=np.asarray(x,dtype=float)
 if rhs.ndim!=1 or not np.all(np.isfinite(rhs)):raise ValueError('invalid inner RHS')
 u=np.zeros_like(rhs);r=rhs.copy();trace=[];calls=0;steps=0;early=False
 def finish():
  if stats is not None:
   stats['calls']+=1;stats['steps']+=steps;stats['factor_applications']+=calls;stats['velocity_applications']+=steps;stats['early_returns']+=int(early);stats['step_histogram'][str(steps)]+=1;stats['last_trace']=trace
  return u
 if not np.any(r):return finish()
 z=precondition(r);calls+=1;p=z.copy();rz=float(r@z);initial=rz
 if not np.isfinite(rz) or rz<=0:raise ValueError('inner nonpositive factor work')
 trace.append(dict(step=0,relative_preconditioned_residual=1.))
 for iteration in range(8):
  Ap=action(p);curvature=float(p@Ap)
  if not np.isfinite(curvature) or curvature<=0:raise ValueError('inner nonpositive physical curvature')
  alpha=rz/curvature;u=u+alpha*p;r=r-alpha*Ap;steps=iteration+1
  if not np.all(np.isfinite(u)) or not np.all(np.isfinite(r)):raise ValueError('nonfinite inner action')
  if not np.any(r):early=steps<8;trace.append(dict(step=steps,relative_preconditioned_residual=0.));break
  if steps==8:break
  z=precondition(r);calls+=1;next_rz=float(r@z)
  if not np.isfinite(next_rz) or next_rz<=0:raise ValueError('inner nonpositive factor work')
  relative=float(np.sqrt(next_rz/initial));trace.append(dict(step=steps,relative_preconditioned_residual=relative))
  if relative<=ETA:early=True;break
  p=z+(next_rz/rz)*p;rz=next_rz
 return finish()
