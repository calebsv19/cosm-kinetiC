import tempfile
import unittest
from pathlib import Path
import numpy as np
from coupling.vertical_reference_report import dense, load, pack, restrict_vertical, validate, winds
from coupling.velocity import sample_mac
from coupling.isolated_qualification import PROBES


def affine(grid):
    x,y,z=grid
    faces=[]
    for axis in range(3):
        shape=[z,y,x];shape[2-axis]+=1
        k,j,i=np.indices(shape)
        faces.append((i+(axis!=0)*.5)/x+2*(j+(axis!=1)*.5)/y+3*(k+(axis!=2)*.5)/z)
    k,j,i=np.indices((z,y,x))
    energy=(1+(i+.5)/x+(j+.5)/y+(k+.5)/z)/(x*y*z)
    return dict(energy_j=energy.ravel(),smoke_kg=energy.ravel()*1e-6,face_velocity_m_s=pack(faces))


class VerticalReferenceTests(unittest.TestCase):
    def test_affine_face_area_and_extensive_restriction(self):
        a=affine([8,8,16]);b=restrict_vertical(a,8);expected=affine([8]*3)
        for key in b:np.testing.assert_allclose(b[key],expected[key],rtol=0,atol=1e-15)
        self.assertAlmostEqual(float(a['energy_j'].sum()),float(b['energy_j'].sum()),places=14)
        for f,g in zip(dense(b['face_velocity_m_s'],[8]*3),dense(expected['face_velocity_m_s'],[8]*3)):
            np.testing.assert_array_equal(f,g)

    def test_native_anisotropic_probe_matches_retained_sampler(self):
        a=affine([8,8,16]);v=winds(a,[8,8,16])
        for name,p in PROBES.items():
            np.testing.assert_allclose(v[name],sample_mac(a['face_velocity_m_s'],[8,8,16],[1]*3,p,solid_floor=True,lateral_open=True),rtol=0,atol=1e-15)

    def test_timing_shape_resource_and_corrupt_field_refusal(self):
        s=dict(qualification_profile='flow_vertical_fixed_scalar_v1',grid=[128,128,256],dt_s=.005,time_s=3.2,
               scalar_dt_s=.00125,flow_steps=640,scalar_cycles=2560,scalar_work_cells=2560*128*128*256,
               max_scalar_work_cells=12000000000,scalar_transport_scheme='muscl_minmod',maximum_divergence=0,
               maximum_pressure_residual=0,heat_balance_j=0,smoke_balance_kg=0,peak_temperature_k=310,
               memory_limit_bytes=4294967296,memory_peak_bytes=1000)
        validate(s)
        for key,value in [('grid',[128]*3),('time_s',.1),('scalar_cycles',640),('dt_s',.0025),('memory_peak_bytes',4294967297)]:
            with self.assertRaises(ValueError):validate(dict(s,**{key:value}))
        with tempfile.TemporaryDirectory() as root:
            p=Path(root)/'field.bin';np.array([-1.]).tofile(p)
            with self.assertRaises(ValueError):load(p,[8,8,16])


if __name__=='__main__':unittest.main()
