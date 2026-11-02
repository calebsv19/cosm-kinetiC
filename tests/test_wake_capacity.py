"""Opt-in wake capacity preserves ordinary admission and exact journal values."""
import os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.wind_presets import configuration
from coupling.journal import Journal
from open_atmosphere import validate,configuration as native_config,SCHEMA

class WakeCapacity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
        f=configuration(workers,'abrupt_gust')['fluid']
        f.update(capacity_profile='wake80/v1',grid=[80]*3,momentum_dt_s=.0025,
                 initial_face_velocity_m_s=[0.]*(3*80**3+3*80**2),
                 initial_energy_j=[0.]*80**3,initial_smoke_kg=[0.]*80**3,
                 solid_box={'schema':'physics_sim_stationary_box/v1','lower_m':[.375]*3,'upper_m':[.625]*3,'velocity_boundary':'no_slip','thermal_boundary':'insulated'},
                 resource_limits={'max_cells':80**3,'scalar_work_cells':1000000000,'numerical_bytes':512*1024**2,'cache_pressure_operator':True,'native_request_bytes':256*1024**2,'native_output_bytes':256*1024**2})
        f['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
        cls.request=dict(f,schema=SCHEMA,state=None,steps=[])
    def test_admit_and_bind_profile(self):
        validate(self.request)
        self.assertEqual(native_config(self.request)['capacity_profile'],'wake80/v1')
    def test_ordinary_cap_preserved(self):
        request=dict(self.request);request.pop('capacity_profile')
        with self.assertRaises(ValueError):validate(request)
    def test_profile_refusals(self):
        for update in ({'capacity_profile':'unknown'},{'grid':[96]*3},{'length_m':[2,1,1]},{'momentum_dt_s':.005},{'wind_forcing':{}},{'solid_box':{}},{'wind_forcing':[]},{'solid_box':None}):
            with self.subTest(update=update),self.assertRaises(ValueError):validate(dict(self.request,**update))
        with self.assertRaises(ValueError):validate(self.request,movie=True)
    def test_lossless_encoding_and_separate_cap(self):
        a=Journal('lossless_f64_zlib/v1');b=Journal('lossless_f64_zlib_1g/v1')
        value={'energy_j':[float(i)/7 for i in range(512)],'signed':[-0.,1e-300]}
        self.assertEqual(a.encode(value),b.encode(value));self.assertEqual(b.decode(a.encode(value)),value)
        self.assertEqual(a.limit,512*1024**2);self.assertEqual(b.limit,1024**3)
        with self.assertRaises(ValueError):b.decode(b.encode(value)+b'junk')
if __name__=='__main__':unittest.main()
