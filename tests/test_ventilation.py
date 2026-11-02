"""Native channel/paired-X event recovery and independent protocol refusals."""
import copy,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.wind_presets import configuration
from coupling.driven_wind import integrated_inlet
from coupling.ventilation import CHANNEL,BALANCED
from coupling.open_boundaries import face
from open_atmosphere import run,validate,SCHEMA
from numeric_digest_stream import sealed

class VentilationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='ventilation-');self.addCleanup(self.temp.cleanup)
        prior=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT')
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(Path(self.temp.name)/'evidence')
        self.addCleanup(lambda:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None) if prior is None else os.environ.__setitem__('PHYSICS_SIM_EXPERIMENT_ROOT',prior))
        self.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def request(self,profile):
        f=configuration(self.workers,profile)['fluid']
        f['wind_forcing']['knots']=[{'step':0,'speed_m_s':.3},{'step':5,'speed_m_s':-.3 if profile=='balanced_reversal' else 0},{'step':10,'speed_m_s':0}]
        e=[0.]*512;m=[0.]*512;e[0]=1e-6;m[0]=1e-12
        return dict(f,schema=SCHEMA,state=None,steps=[{'energy_j':e,'smoke_kg':m}]*21)
    def test_native_restart_across_off_and_reversal(self):
        for profile in ('channel_patch','channel_patch_box','balanced_reversal'):
            r=self.request(profile);full=run(r,self.workers[2]);first=run(dict(r,steps=r['steps'][:7]),self.workers[2]);second=run(dict(r,state=first['state'],steps=r['steps'][7:]),self.workers[2])
            self.assertEqual(full['state'],second['state'])
            # Invocation start and allocation peak describe the call, not the final physical state.
            for key in full['fields']:
                if key not in ('start_time_s','numerical_peak_bytes'):self.assertEqual(full['fields'][key],second['fields'][key],key)
            self.assertAlmostEqual(full['wind_observation']['air_in_m3'],full['wind_observation']['air_out_m3'],places=12)
            if profile=='balanced_reversal':
                self.assertLess(first['fields']['face_velocity_m_s'][face([8]*3,0,0,3,3,True)],0)
                self.assertLess(first['fields']['face_velocity_m_s'][face([8]*3,0,8,3,3,True)],0)
    def test_patch_and_full_have_equal_prescribed_volume(self):
        a=configuration(self.workers,'channel_full')['fluid'];b=configuration(self.workers,'channel_patch')['fluid']
        self.assertAlmostEqual(integrated_inlet(a,640),1.4,places=13)
        self.assertAlmostEqual(integrated_inlet(b,640),1.4,places=13)
    def test_balanced_signed_linear_absolute_integral(self):
        r=self.request('balanced_reversal');r['wind_forcing'].update(interpolation='linear',knots=[{'step':0,'speed_m_s':.5},{'step':100,'speed_m_s':-.5}]);validate(r)
        self.assertAlmostEqual(integrated_inlet(r,101),25.5*.875*.005,places=13)
    def test_aperture_and_reservoir_admission(self):
        for change in ('misaligned','outside','nonzero_pressure','moving_initial','unknown_key'):
            r=self.request('channel_patch');w=r['wind_forcing']
            if change=='misaligned':w['inlet_patch_m']['lower_yz_m'][0]=.26
            if change=='outside':w['inlet_patch_m']['upper_yz_m'][0]=1.1
            if change=='nonzero_pressure':r['boundary_policy']['pressure_datum_pa'][1]=1
            if change=='moving_initial':r['initial_face_velocity_m_s'][10]=.1
            if change=='unknown_key':w['confined']=False
            with self.assertRaises(ValueError):validate(r)
    def test_legacy_cannot_accept_signed_speed_or_pair(self):
        for key,value in (('inlet','x_pair'),('knots',[{'step':0,'speed_m_s':-.3}])):
            r=self.request('channel_patch');r['wind_forcing'].pop('inlet_patch_m');r['wind_forcing']['schema']='physics_sim_driven_x_inlet/v1';r['wind_forcing'][key]=value
            with self.assertRaises(ValueError):validate(r)
    def test_channel_closed_face_and_flux_tamper(self):
        r=self.request('channel_patch');cp=run(dict(r,steps=r['steps'][:7]),self.workers[2])['state']
        for flux in (False,True):
            c=copy.deepcopy(cp)
            if flux:c['data']['boundary_fluxes'][64]=1e-8
            else:c['data']['face_velocity_m_s'][face([8]*3,1,2,0,2,True)]=.01
            with self.assertRaises(ValueError):validate(dict(r,state=sealed(c),steps=[]))
    def test_changed_configuration_and_integral_tamper(self):
        r=self.request('balanced_reversal');cp=run(dict(r,steps=r['steps'][:7]),self.workers[2])['state'];c=copy.deepcopy(r);c['wind_forcing']['knots'][1]['speed_m_s']=-.2
        with self.assertRaises(ValueError):validate(dict(c,state=cp,steps=[]))
        c=copy.deepcopy(cp);c['data']['wind_inlet_volume_m3']+=.01
        with self.assertRaisesRegex(ValueError,'inlet volume'):validate(dict(r,state=sealed(c),steps=[]))

if __name__=='__main__':unittest.main()
