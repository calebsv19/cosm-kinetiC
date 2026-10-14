"""Independent native-state/candidate readback; imports no solver/report modules."""
import argparse,json,math,hashlib
from pathlib import Path
import numpy as np

def digest(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()

def inspect(prefix, original_support=None):
 prefix=str(prefix);paths={k:Path(prefix+'.'+k) for k in ['accepted.bin','candidate.bin','source.bin','rejection.json']}
 d=json.loads(paths['rejection.json'].read_text());u=np.fromfile(paths['accepted.bin'],dtype='u8',count=12);v=np.fromfile(paths['accepted.bin'],dtype='f8',offset=96,count=26)
 if len(u)!=12 or len(v)!=26 or int(u[0])!=0x4d4f4d5354415445 or int(u[1])!=1 or int(u[11])!=31 or not np.isfinite(v).all():raise ValueError('native header/profile')
 x,y,z=map(int,u[2:5]);n=x*y*z;vc=3*n+x*y+x*z+y*z;bf=2*(x*y+x*z+y*z)
 if (int(u[5])!=n or int(u[6])!=vc or int(u[7])!=bf or d['grid']!=[x,y,z] or d['accepted_steps']!=int(u[8])
     or d['accepted_time_s']!=v[19] or d['flow_dt_s']!=v[9] or d['heat_gate_relative']!=.1):raise ValueError('metadata/header identity')
 if paths['accepted.bin'].stat().st_size!=304+8*(vc+3*n+4*bf):raise ValueError('accepted byte count')
 a=np.memmap(paths['accepted.bin'],dtype='f8',mode='r',offset=304);velocity=a[:vc];pressure=a[vc:vc+n];heat=a[vc+n:vc+2*n];smoke=a[vc+2*n:vc+3*n];flux=a[vc+3*n:].reshape(4,bf)
 if not np.isfinite(a).all() or (heat<0).any() or (smoke<0).any() or (flux<0).any() or np.any(velocity[2*n:2*n+x*y]) or np.any(flux[:,:x*y]):raise ValueError('accepted finite/nonnegative/ground invariants')
 def divergence(w):
  xx=w[:n].reshape(z,y,x);yy=w[n:2*n].reshape(z,y,x);zz=w[2*n:3*n].reshape(z,y,x)
  dx=np.empty_like(xx);dy=np.empty_like(yy);dz=np.empty_like(zz)
  dx[:,:,:-1]=xx[:,:,1:]-xx[:,:,:-1];dx[:,:,-1]=w[3*n+x*y:3*n+x*y+y*z].reshape(z,y)-xx[:,:,-1]
  dy[:,:-1,:]=yy[:,1:,:]-yy[:,:-1,:];dy[:,-1,:]=w[3*n+x*y+y*z:].reshape(z,x)-yy[:,-1,:]
  dz[:-1]=zz[1:]-zz[:-1];dz[-1]=w[3*n:3*n+x*y].reshape(y,x)-zz[-1]
  return dx/(v[0]/x)+dy/(v[1]/y)+dz/(v[2]/z)
 accepted_div=float(np.max(np.abs(divergence(velocity))))
 heat_balance=float(heat.sum()-v[20]-v[22]-flux[0].sum()+flux[1].sum());smoke_balance=float(smoke.sum()-v[21]-v[23]-flux[2].sum()+flux[3].sum())
 if accepted_div>=1e-8 or abs(heat_balance)>=1e-9 or abs(smoke_balance)>=1e-15:raise ValueError('accepted divergence/accounting')
 source=np.memmap(paths['source.bin'],dtype='f8',mode='r')
 if len(source)!=2*n or not np.isfinite(source).all() or (source<0).any():raise ValueError('rejected source')
 source_verified=False
 if original_support is not None:
  if digest(original_support)!='6db4841abd9c7418d56f100203d2727aca6ac8fdd8384feab7392dddb2172f30' or [x,y,z]!=[128,128,256]:raise ValueError('original source identity')
  frame=d['frame_zero_based'];step=d['step_zero_based']
  if not 0<=frame<32 or not 0<=step<20 or int(u[8])!=20*frame+step:raise ValueError('source/window/accepted-step identity')
  support=np.memmap(original_support,dtype='f8',mode='r',shape=(32,2,16,128,128))
  expected=(np.repeat(support[frame],2,axis=1)*.5)/20
  actual=source.reshape(2,256,128,128)
  if not np.array_equal(actual[:,:32],expected) or np.any(actual[:,32:]):raise ValueError('original rejected-step source mapping')
  source_verified=True
 candidate=np.memmap(paths['candidate.bin'],dtype='f8',mode='r')
 if len(candidate)!=vc+2*n:raise ValueError('candidate byte count')
 def sample(axis,i,j,k):
  i=max(0,min(i,x-(axis!=0)));j=max(0,min(j,y-(axis!=1)));sign=1
  if axis==2:
   if k<0:k=-k;sign=-1
   if k>z:k=2*z-k
  else:
   if k<0:k=-k-1;sign=-1
   k=min(k,z-1)
  if axis==0 and i==x:q=3*n+x*y+k*y+j
  elif axis==1 and j==y:q=3*n+x*y+y*z+k*x+i
  else:q=axis*n+(k*y+j)*x+i
  return sign*velocity[q]
 axis,*p=d['maximum_rate_face'];rate=0
 def speed(b,point):
  hi=point.copy();hi[b]+=1
  if axis==b:return .5*(sample(b,*point)+sample(b,*hi))
  lo=hi.copy();lo[axis]-=1;return .5*(sample(b,*hi)+sample(b,*lo))
 for b,size in enumerate((x,y,z)):
  lo=p.copy();lo[b]-=1;h=v[b]/size
  rate+=(max(speed(b,p),0)+max(-speed(b,lo),0))/h+2*(v[4]/v[3])/(h*h)
 courant=float(v[9]*rate)
 if d['candidate_is_rejected_scratch'] and abs(courant-d['momentum_courant'])>1e-12:raise ValueError('independent momentum CFL')
 candidate_div=None;true_residual=None
 if d['exact_momentum_check']=='projection_divergence':
  candidate_div=float(np.max(np.abs(divergence(candidate[:vc]))))
  if candidate_div<1e-8 or abs(candidate_div-d['projection_maximum_divergence'])>1e-10*max(1,candidate_div):raise ValueError('independent projection refusal')
 if d['pressure_attempted'] and d['exact_momentum_check'] not in ('pressure_cache_boundary','pressure_solver_allocation'):
  # Same continuum stencil, evaluated in sorted CSR column order. Unit domain
  # dyadic grids make the assembled diagonal coefficients exact in binary.
  if not np.array_equal(v[:3],[1,1,1]):raise ValueError('unit-domain reference')
  p=candidate[vc:vc+n].reshape(z,y,x);rhs=candidate[vc+n:];op=np.zeros_like(p);ks=(float(x*x),float(y*y),float(z*z))
  op[1:]+=(-ks[2])*p[:-1];op[:,1:]+=(-ks[1])*p[:,:-1];op[:,:,1:]+=(-ks[0])*p[:,:,:-1]
  diagonal=np.full_like(p,2*sum(ks));diagonal[:,:,0]+=ks[0];diagonal[:,:,-1]+=ks[0];diagonal[:,0,:]+=ks[1];diagonal[:,-1,:]+=ks[1];diagonal[0]-=ks[2];diagonal[-1]+=ks[2]
  op+=diagonal*p;op[:,:,:-1]+=(-ks[0])*p[:,:,1:];op[:,:-1]+=(-ks[1])*p[:,1:];op[:-1]+=(-ks[2])*p[1:]
  scale=float(np.linalg.norm(rhs));true_residual=float(np.linalg.norm(op.ravel()-rhs)/(scale if scale>=1e-30 else 1))
 if d['candidate_is_rejected_scratch'] and d['accepted_guard_available'] and (not d['accepted_unchanged'] or d['accepted_fingerprint_before']!=d['accepted_fingerprint_after']):raise ValueError('accepted state changed at refusal')
 return dict(schema='momentum_capture_readback_v1',diagnostics=d,accepted_maximum_divergence=accepted_div,accepted_heat_balance_j=heat_balance,accepted_smoke_balance_kg=smoke_balance,
             independently_recomputed_courant=courant,candidate_maximum_divergence=candidate_div,candidate_current_true_pressure_residual=true_residual,
             solver_reported_residual_is_last_reported_and_may_precede_terminal_iterate=True,original_rejected_step_source_verified=source_verified,
             courant_recomputed_from_poststep_state=not d['candidate_is_rejected_scratch'],
             hashes={k:digest(p) for k,p in paths.items()},accepted_state_valid=True)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('prefix');p.add_argument('output');p.add_argument('--original-source-support');args=p.parse_args();result=inspect(args.prefix,args.original_source_support);Path(args.output).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
