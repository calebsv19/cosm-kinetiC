"""Native compensated thermal restart, signed accounting and refusal controls."""
import copy,json,math,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from open_atmosphere import run,validate
from numeric_digest_stream import sealed
from coupling.inert_cases import heater_request,recipe
from coupling.fluid_packets import run as packet_run
class InertCheckpointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
    def setUp(self):
        root=Path(os.environ['INERT_TEST_ROOT'])/self._testMethodName;root.mkdir(parents=True,exist_ok=False)
        old=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(root/'attempts')
        self.addCleanup(lambda:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None) if old is None else os.environ.__setitem__('PHYSICS_SIM_EXPERIMENT_ROOT',old))
    def test_exact_restart_during_source_and_previous_heater(self):
        r=heater_request(self.workers);full=run(r,self.workers[2]);first=run(dict(r,steps=r['steps'][:61]),self.workers[2])
        end=run(dict(r,state=first['state'],steps=r['steps'][61:]),self.workers[2])
        self.assertEqual(full['state'],end['state'])
        o=full['state']['data']['inert_object']
        self.assertGreater(o['temperature_k']-300,.001)
        previous=float(os.environ['INERT_PREVIOUS_TRANSFER_J'])
        self.assertEqual(o['gas_to_object_j'],previous)
        self.assertEqual(run(dict(r,state=end['state'],steps=[]),self.workers[2])['state'],end['state'])
    def test_exchange_off_matches_original_native_path(self):
        r=heater_request(self.workers,steps=100,h=0);off=run(r,self.workers[2]);plain=copy.deepcopy(r);plain.pop('inert_object');legacy=run(plain,self.workers[2])
        data=copy.deepcopy(off['state']['data']);obj=data.pop('inert_object');self.assertEqual(data,legacy['state']['data']);self.assertEqual(obj['gas_to_object_j'],0.)
        self.assertEqual(obj['temperature_k'],300.)
    def test_packet_boundaries_and_signed_cooling(self):
        r=heater_request(self.workers,steps=20);full=run(r,self.workers[2]);config={k:v for k,v in r.items() if k not in ('schema','state','steps')}
        packet=packet_run(config,None,r['steps'],self.workers[2],7);self.assertEqual(full['state'],packet['state'])
        cool=heater_request(self.workers,steps=40);cool['inert_object']['initial_temperature_k']=305.
        for step in cool['steps']:step['energy_j']=[0.]*4096
        result=run(cool,self.workers[2]);self.assertLess(result['state']['data']['inert_object']['gas_to_object_j'],0.)
        self.assertLess(abs(result['budgets']['energy_j']['balance']),1e-9)
    def test_resealed_object_tamper_and_configuration_refused(self):
        r=heater_request(self.workers,steps=2);s=run(r,self.workers[2])['state']
        for field,delta in [('gas_to_object_j',1.),('temperature_correction_k',.001),('steps',1),('time_s',.1)]:
            c=copy.deepcopy(s);c['data']['inert_object'][field]+=delta;c=sealed(c)
            with self.assertRaises(ValueError):validate(dict(r,state=c))
        for key,value in [('support_depth_m',.1),('axis',3),('capacity_j_k',0),('side',0)]:
            c=copy.deepcopy(r);c['inert_object'][key]=value
            with self.assertRaises(ValueError):validate(c)
        c=copy.deepcopy(r);c['inert_object']['h_w_m2_k']=3
        with self.assertRaisesRegex(ValueError,'checkpoint/config'):validate(dict(c,state=s))
        c=copy.deepcopy(s);c['data']['inert_object']['gas_to_object_j']+=1.;c['data']['inert_object']['temperature_k']+=.1;c=sealed(c)
        from open_atmosphere import accept_fields
        fields=copy.deepcopy(run(r,self.workers[2])['fields']);fields['state']=c['data'];fields['inert_object']=c['data']['inert_object']
        with self.assertRaisesRegex(ValueError,'conservation'):accept_fields(r,fields,s['worker_sha256'])
if __name__=='__main__':unittest.main()
