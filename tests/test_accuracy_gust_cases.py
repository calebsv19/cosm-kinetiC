import json,os,sys,unittest
from fractions import Fraction
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.accuracy_gust_cases import configuration,CASES
class AccuracyGustCases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        w=json.loads(Path(os.environ["REPLAY_WORKERS"]).read_text())
        cls.workers=(w["growth_root"],*[w["binaries"][k]["path"] for k in ("fire","fluid","clock")])
    def test_fixed_physics_events_and_source_support(self):
        configs=[configuration(self.workers,n) for n in CASES];a=configs[0]
        for c in configs:
            for key in ('source','source_delivery'):self.assertEqual(c[key],a[key])
            f=c['fluid'];self.assertEqual(c['periods'][2],[1,5]);self.assertEqual(c['windows'],16)
            for key in ('properties','inert_object','solid_box','buoyancy'):self.assertEqual(f[key],a['fluid'][key])
            events=[k['step']*Fraction(*c['periods'][1]) for k in f['open_domain_forcing']['knots']]
            self.assertEqual(events,[0,Fraction(2,5),Fraction(6,5)])
            self.assertEqual(c['receiver']['injection_depth_m'],.125)
            self.assertEqual(f['resource_limits']['max_cells'],f['grid'][0]**3)
    def test_named_control_refuses(self):
        with self.assertRaises(ValueError):configuration(('g','f','w','c'),'gust128')

class AccuracyWakeGeometry(unittest.TestCase):
    def test_64_grid_uses_same_physical_regions(self):
        import numpy as np
        from coupling.open_weather_diagnostics import observe
        from coupling.flow_spatial_fields import pack
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text());workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
        c=configuration(workers,'gust64');n=64
        u=pack((np.full((n,n,n+1),.2),np.zeros((n,n+1,n)),np.zeros((n+1,n,n))))
        d=observe({'face_velocity_m_s':u,'energy_j':np.full(n**3,1/n**3),'smoke_kg':np.zeros(n**3)},c)
        for key in ('x_min_wake','x_max_wake'):
            self.assertEqual(d[key]['cells'],16**3);self.assertAlmostEqual(d[key]['mean_u_x_m_s'],.2)
            self.assertAlmostEqual(d[key]['energy_j'],1/64);self.assertEqual(d[key]['vorticity_rms_s_inv'],0)

if __name__=='__main__':unittest.main()
