"""Native schedule event recovery, independent commanded volume and admission."""
import copy,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.driven_wind_qualification import configuration
from coupling.driven_wind import integrated_inlet
from open_atmosphere import run,validate,SCHEMA
class DrivenWindTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='driven-wind-');self.addCleanup(self.temp.cleanup)
        prior=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(Path(self.temp.name)/'evidence')
        self.addCleanup(lambda:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None) if prior is None else os.environ.__setitem__('PHYSICS_SIM_EXPERIMENT_ROOT',prior))
        self.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def request(self,profile):
        f=configuration(self.workers,profile)['fluid'];zero={'energy_j':[0.]*512,'smoke_kg':[0.]*512}
        return dict(f,schema=SCHEMA,state=None,steps=[zero]*161)
    def test_native_restart_across_event(self):
        for profile in ('continuous-negative','abrupt-gust','ramped-gust'):
            r=self.request(profile);full=run(r,self.workers[2]);first=run(dict(r,steps=r['steps'][:79]),self.workers[2])
            second=run(dict(r,state=first['state'],steps=r['steps'][79:]),self.workers[2])
            self.assertEqual(full['state'],second['state'])
            self.assertLess(abs(full['wind_observation']['air_in_m3']-full['wind_observation']['air_out_m3']),1e-11)
    def test_equal_integrated_gusts_and_late_off(self):
        a=self.request('abrupt-gust');b=self.request('ramped-gust')
        self.assertAlmostEqual(integrated_inlet(a,640),.35,places=14)
        self.assertAlmostEqual(integrated_inlet(a,640),integrated_inlet(b,640),places=14)
    def test_refuse_unsupported_or_mutated_configuration(self):
        r=self.request('abrupt-gust');validate(r)
        for change in ('wrong_schema','periodic','bad_height','unordered','negative_speed'):
            c=copy.deepcopy(r);w=c['wind_forcing']
            if change=='wrong_schema':w['schema']='unknown'
            if change=='periodic':c['boundary_policy']['horizontal']='periodic_xy'
            if change=='bad_height':w['ramp_height_m']=0
            if change=='unordered':w['knots'][1]['step']=0
            if change=='negative_speed':w['knots'][1]['speed_m_s']=-.5
            with self.assertRaises(ValueError):validate(c)
    def test_changed_schedule_and_counter_tamper_refused(self):
        r=self.request('continuous-negative');r['steps']=r['steps'][:2];cp=run(r,self.workers[2])['state']
        c=copy.deepcopy(r);c['wind_forcing']['knots'][0]['speed_m_s']=.4
        with self.assertRaises(ValueError):validate(dict(c,state=cp))
        from numeric_digest_stream import sealed
        c=copy.deepcopy(cp);c['data']['wind_inlet_volume_m3']+=.1;c=sealed(c)
        with self.assertRaisesRegex(ValueError,'inlet volume'):validate(dict(r,state=c))
if __name__=='__main__':unittest.main()
