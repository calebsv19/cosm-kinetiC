"""Pressure-only sampled coverage augmentation; all physical equations retained."""
import json,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_pressure_coverage import basis
from cfd_reference3d_pressure_complement10 import polynomial_basis,BalancedPressure
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve as old_diagnostic_reserve
COEFFICIENT_PATH=Path(__file__).with_name('cfd_reference3d_pressure_coverage16_coefficients.json')

def reserve(nv,np_):
 if not isinstance(nv,int) or not isinstance(np_,int) or nv<1 or np_<16:raise ValueError('invalid pressure16 reservation')
 return int(8*(3*np_*16+8*np_+4*nv+8*16**2)+2*2**20+8*(8*np_*58+6*58**2)+8*2**20)

def diagnostic_reserve(nv,np_):return old_diagnostic_reserve(nv,np_)+8*(4*nv*16+12*np_*16)+8*2**20

def make_basis(centers,volumes,mu,length,lo,hi):
 data=json.loads(COEFFICIENT_PATH.read_text());C=np.asarray(data['coefficients'],dtype=float)
 if C.shape!=(58,6) or not np.all(np.isfinite(C)) or np.linalg.norm(C[:10])>1e-12 or np.linalg.norm(C.T@C-np.eye(6))>1e-10:raise ValueError('invalid frozen pressure coefficients')
 Q,meta=basis(centers,volumes,mu,length,lo,hi)
 if meta['kept']!=data['basis_names'] or Q.shape[1]!=58:raise ValueError('transport pressure basis names/rank mismatch')
 old=polynomial_basis(centers,volumes,mu,length);root=np.sqrt(volumes/mu);Z=np.column_stack((old,(Q@C)/root[:,None]));error=float(np.linalg.norm(Z.T@((volumes/mu)[:,None]*Z)-np.eye(16)))
 if error>1e-10 or not np.array_equal(Z[:,:10],old):raise ValueError('pressure16 rank/old directions lost')
 return Z,dict(kind='original_ten_plus_six_frozen_sampled_pressure_directions',columns=16,original_ten_retained_bitwise=True,constant_direction_retained=True,mass_orthogonality_error=error,coefficient_sha256=hashlib.sha256(COEFFICIENT_PATH.read_bytes()).hexdigest(),basis_names=meta['kept'],scope='transport fixed sampled coefficients only; not full Schur spectrum or physicalpressure reduction')

def build(mesh,volumes,mu,length,coupling,pressure,velocity,lo=None,hi=None,sample=None):
 if lo is None or hi is None:raise ValueError('explicit unchanged body bounds required')
 centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T;Z,meta=make_basis(centers,volumes,mu,length,lo,hi);W=np.empty_like(Z)
 for j in range(16):
  W[:,j]=coupling.T@velocity(coupling@Z[:,j])-pressure@Z[:,j]
  if sample and (j+1)%4==0:sample('pressure16_columns_'+str(j+1))
 obj=BalancedPressure(Z,W,10*mu/volumes);obj.metadata.update(kind='balanced_pressure_sampled_coverage16_complement10',complementary_mass_inverse_scale=10.,coarse_correction_scale=1.,coverage_basis=meta,coarse_reservation_bytes=reserve(coupling.shape[0],len(volumes)),scope='pressure preconditioner only; original all pressureunknowns/gauge/equations/fullFE retained');return obj
