import math,sys,unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.flow_spatial_report import validate
from coupling.flow_spatial_fields import dense,pack,restrict_faces,restrict_extensive,centered
from coupling.matched_heat_report import compare

def native(n):
    return dict(qualification_profile='flow_spatial_fixed_scalar_v1',grid=n,dt_s=.005,time_s=3.2,
                scalar_dt_s=.00125,flow_steps=640,scalar_cycles=2560,scalar_work_cells=2560*n**3,
                max_scalar_work_cells=8000000000,scalar_transport_scheme='muscl_minmod',
                maximum_divergence=1e-12,maximum_pressure_residual=1e-12,heat_balance_j=0,
                smoke_balance_kg=0,peak_temperature_k=310,memory_limit_bytes=2147483648,memory_peak_bytes=1000)

class FlowSpatialReport(unittest.TestCase):
    def test_fixed_matched_identity_and_invariants(self):
        for n in (64,128):
            validate(native(n),n)
            for key,value in [('scalar_cycles',783),('flow_steps',1280),('dt_s',.0025),
                              ('scalar_dt_s',.005),('maximum_divergence',math.nan),
                              ('heat_balance_j',1e-8),('memory_peak_bytes',2147483649)]:
                s=native(n);s[key]=value
                with self.assertRaises(ValueError):validate(s,n)

    def test_normal_face_area_restriction_preserves_affine_incompressible_flow(self):
        n=8;m=2*n
        k,j,i=np.indices((m,m,m+1));x=-.5*i/m
        k,j,i=np.indices((m,m+1,m));y=-.5*j/m
        k,j,i=np.indices((m+1,m,m));z=k/m
        v=pack([x,y,z]);f=restrict_faces(v,n)
        k,j,i=np.indices((n,n,n+1));np.testing.assert_array_equal(f[0],-.5*i/n)
        k,j,i=np.indices((n,n+1,n));np.testing.assert_array_equal(f[1],-.5*j/n)
        k,j,i=np.indices((n+1,n,n));np.testing.assert_array_equal(f[2],k/n)
        divergence=n*(np.diff(f[0],axis=2)+np.diff(f[1],axis=1)+np.diff(f[2],axis=0))
        np.testing.assert_array_equal(divergence,np.zeros((n,n,n)))
        for a,b in zip(dense(pack(f),n),f):np.testing.assert_array_equal(a,b)

    def test_extensive_restriction_and_unchanged_heat_gate(self):
        n=32;m=2*n;p=np.arange(n**3,dtype=float).reshape(n,n,n)/4096
        fine=np.repeat(np.repeat(np.repeat(p,2,axis=0),2,axis=1),2,axis=2)/8
        np.testing.assert_array_equal(restrict_extensive(fine.ravel(),n),p.ravel())
        b=np.zeros(n**3);q=(4*n+12)*n+12;b[q]=10;a=b.copy();a[q]=9;a[-1]=1000
        result=compare(a,b,n)
        self.assertEqual(result['local_heat_relative_l1'],.1);self.assertTrue(result['local_heat_gate_passed'])
        a[q]=8.99;self.assertFalse(compare(a,b,n)['local_heat_gate_passed'])
if __name__=='__main__':unittest.main()
