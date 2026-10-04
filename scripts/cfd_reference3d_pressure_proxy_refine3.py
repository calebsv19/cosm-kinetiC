"""Stronger fixed symmetric pressure proxy; outer velocity action remains CG8."""
from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor
from cfd_reference3d_p3_cg8_scalar import triple,work_reserve as old_work_reserve,fresh_admission as old_admission
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve as old_diagnostic_reserve

def work_reserve(nv,nc):return old_work_reserve(nv,nc)+8*8*nv

def diagnostic_reserve(nv,np_):return old_diagnostic_reserve(nv,np_)+8*nv*10

def fresh_admission(factor,outer_bytes,pressure_bytes,**kwargs):
 a=old_admission(factor,outer_bytes,pressure_bytes,**kwargs);new=work_reserve(factor.n,factor.Z.shape[1]);delta=new-a['distributed_velocity_work_reservation_bytes'];a['distributed_velocity_work_reservation_bytes']=new;a['basis_reservation_bytes']+=delta;a['estimated_numeric_stage_bytes']+=delta;a['numeric_stage_admitted']=a['estimated_numeric_stage_bytes']<=1800*2**20;return a

class PressureProxyRefine3Factor(DistributedP3CG8ScalarPressureFactor):
 def __init__(self,*args,**kwargs):
  super().__init__(*args,**kwargs)
  self.metadata.update(kind='P3_CG8_velocity_three_balanced_pressure_corrections',pressure_proxy_fixed_balanced_steps=3,distributed_work_reservation_bytes=work_reserve(self.n,self.Z.shape[1]),pressure_velocity_proxy='three fixed original-Av corrections of validated balanced P3/local triple; original full pressure modes and equations retained')
 def pressure_solve(self,x):
  if not self._handle:raise ValueError('closed pressure correction')
  return triple(lambda u:self.owner@u,self.linear_pressure.solve,x)
