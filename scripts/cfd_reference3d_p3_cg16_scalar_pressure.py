"""Nonlinear flexible velocity correction with separately fixed pressure proxy."""
from cfd_reference3d_p3_cg16_scalar import DistributedP3CG16ScalarFactor,BalancedSparse,triple,BlockIC0,fresh_admission,work_reserve

class DistributedP3CG16ScalarPressureFactor(DistributedP3CG16ScalarFactor):
    def __init__(self,*args,**kwargs):
        try:
            super().__init__(*args,**kwargs)
            self.linear_pressure=BalancedSparse(self.owner,self.Z,self.coarse_factor,lambda x:triple(lambda u:self.owner@u,lambda r:BlockIC0.solve(self,r),x))
            self.metadata.update(kind='distributed_P3_CG16_flexible_velocity_fixed_pressure_proxy',pressure_velocity_proxy='same P3 Galerkin/Float3 and local triple validated before CG16; unchanged physical pressure/coupling',fixed_pressure_velocity=self.linear_pressure.metadata)
            callback=kwargs.get('stage_callback')
            if callback:callback('fixed_pressure_velocity_ready')
        except BaseException:self.close();raise
    def pressure_solve(self,x):
        if not self._handle:raise ValueError('closed fixed pressure velocity proxy')
        return self.linear_pressure.solve(x)
    def close(self):
        if hasattr(self,'linear_pressure'):del self.linear_pressure
        super().close()
