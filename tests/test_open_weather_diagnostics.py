"""Manufactured-field checks for diagnostic geometry and SI extensive totals."""
import copy,json,os
from pathlib import Path
import sys,unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.open_weather_cases import configuration
from coupling.open_weather_diagnostics import observe,regions,probes
from coupling.flow_spatial_fields import pack

class OpenWeatherDiagnosticsTests(unittest.TestCase):
    def config(self,n):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        return configuration((w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')]),'continuous',n)
    def fields(self,n,u):
        velocity=pack((np.full((n,n,n+1),u),np.zeros((n,n+1,n)),np.zeros((n+1,n,n))))
        return {'face_velocity_m_s':velocity.tolist(),'energy_j':[1/n**3]*n**3,'smoke_kg':[2/n**3]*n**3}
    def test_low_box_both_sides_and_physical_extent(self):
        for n in (16,32):
            c=self.config(n);r=regions(c)
            self.assertEqual(r['x_min_wake'],[[.125,.375],[.375,.625],[.0625,.3125]])
            self.assertEqual(r['x_max_wake'],[[.625,.875],[.375,.625],[.0625,.3125]])
            self.assertEqual(probes(c)['source_return'],[.125,.5,.125])
    def test_constant_field_and_extensive_totals(self):
        for n in (16,32):
            result=observe(self.fields(n,.2),self.config(n))
            for name in ('x_min_wake','x_max_wake'):
                r=result[name];self.assertEqual(r['cells'],(n//4)**3)
                self.assertAlmostEqual(r['mean_u_x_m_s'],.2)
                self.assertAlmostEqual(r['speed_rms_m_s'],.2)
                self.assertEqual(r['vorticity_rms_s_inv'],0)
                self.assertEqual(r['negative_x_volume_fraction'],0)
                self.assertAlmostEqual(r['energy_j'],.25**3)
                self.assertAlmostEqual(r['smoke_kg'],2*.25**3)
    def test_80_capacity_preserves_physical_volume_and_signed_flow(self):
        c=json.loads(Path(os.environ['SCALAR_CAPACITY_CONFIG']).read_text())
        for u in (.2,-.2):
            result=observe(self.fields(80,u),c)
            for name in ('x_min_wake','x_max_wake'):
                r=result[name];self.assertEqual(r['cells'],20**3)
                self.assertAlmostEqual(r['speed_rms_m_s'],.2)
                self.assertAlmostEqual(r['mean_u_x_m_s'],u)
                self.assertEqual(r['vorticity_rms_s_inv'],0)
                self.assertEqual(r['negative_x_volume_fraction'],int(u<0))
                self.assertAlmostEqual(r['energy_j'],.25**3)
                self.assertAlmostEqual(r['smoke_kg'],2*.25**3)
    def test_128_preserves_region_geometry_and_extensive_sums(self):
        c=json.loads(Path(os.environ['SCALAR_CAPACITY_CONFIG']).read_text())
        c['fluid']['grid']=[128]*3
        result=observe(self.fields(128,-.2),c)
        for name in ('x_min_wake','x_max_wake'):
            r=result[name];self.assertEqual(r['cells'],32**3)
            self.assertAlmostEqual(r['mean_u_x_m_s'],-.2)
            self.assertEqual(r['negative_x_volume_fraction'],1)
            self.assertEqual(r['vorticity_rms_s_inv'],0)
            self.assertAlmostEqual(r['energy_j'],.25**3)
            self.assertAlmostEqual(r['smoke_kg'],2*.25**3)
    def test_reversal_does_not_move_regions(self):
        c=self.config(16);r=observe(self.fields(16,-.2),c)
        for name in ('x_min_wake','x_max_wake'):
            self.assertEqual(r[name]['negative_x_volume_fraction'],1)
            self.assertAlmostEqual(r[name]['speed_rms_m_s'],.2)
    def test_nonfinite_field_refuses(self):
        c=self.config(16);f=self.fields(16,.2);f['face_velocity_m_s'][0]=float('nan')
        with self.assertRaises(ValueError):observe(f,c)

if __name__=='__main__':unittest.main()
