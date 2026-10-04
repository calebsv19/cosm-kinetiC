"""Same graph, storage and fixed pressure proxy; tighter safe fill compensation."""
from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor as Parent, fresh_admission, work_reserve
class TightFillBoundFactor(Parent):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs)
        self.metadata.update(kind='distributed_P3_CG8_tight_fill_bound',discarded_fill_bound='min(Frobenius,sqrt(one_norm*infinity_norm)) with conservative roundoff allowance',physical_matrix_changed=False)
