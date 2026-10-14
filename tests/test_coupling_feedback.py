"""Native HOLD admission, prior-checkpoint identity and persisted input consumption."""
from contextlib import closing
import copy,json,os,sqlite3,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.framework import Framework,ref
from fire_fluid_feedback import configuration
from surface_sources.growth_fire_v1 import sealed,canonical
class FeedbackTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='held-return-contract-');self.root=Path(self.temp.name)
        self.prior=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(self.root/'evidence')
        self.addCleanup(self.temp.cleanup);self.addCleanup(self.restore)
        self.config=configuration(os.environ['FIRE_FLUID_GROWTH_ROOT'],os.environ['FIRE_FLUID_FIRE_WORKER'],os.environ['FIRE_FLUID_OPEN_WORKER'],os.environ['FIRE_FLUID_CLOCK_WORKER'])
    def restore(self):
        if self.prior is None:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None)
        else:os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=self.prior
    def test_initial_prior_consumption_and_core_causality_refusals(self):
        host=Framework(self.config,self.root/'store');initial=host.initialize();host.step(0,'first');cp=host.inspect()
        held=cp['evidence']['held_return'];self.assertEqual(held['observation']['joint_checkpoint_digest'],initial['digest'])
        self.assertEqual(held['observation']['checkpoint_tick'],0);self.assertEqual(held['held_window_ticks'],[0,8])
        self.assertEqual(held['wind_input']['tangent_velocity_m_s'][0],[.025,.00625])
        request=dict(host.base_plan,initial_id=initial['id'],initial_validation=ref(initial['evidence']),
            initial_checkpoints=[ref(initial['source']),ref(initial['fluid']['state'])],history=[cp['transition']])
        for field,value in (('return_checkpoint',ref(cp['fluid']['state'])),('return_tick',8),('return_request',9)):
            bad=copy.deepcopy(request);bad['history'][0][field]=value
            with self.assertRaises(ValueError):host.clock(bad)
    def test_explicit_configuration_and_persisted_tamper_refusal(self):
        bad=copy.deepcopy(self.config);bad.pop('return_channel')
        with self.assertRaises(ValueError):Framework(bad,self.root/'bad')
        self.assertFalse((self.root/'bad').exists())
        host=Framework(self.config,self.root/'store');host.initialize();host.step(0,'first');cp=host.inspect()
        cp['evidence']['held_return']['wind_input']['tangent_velocity_m_s'][0][0]+=.01
        cp=sealed(cp)
        with closing(sqlite3.connect(host.path)) as db,db:db.execute('UPDATE checkpoints SET payload=? WHERE revision=1',(canonical(cp).decode(),))
        with self.assertRaisesRegex(ValueError,'prior return identity'):host.inspect()
    def test_staging_failure_keeps_prior_return_and_head(self):
        host=Framework(self.config,self.root/'store');host.initialize();host.step(0,'first');before=host.path.read_bytes()
        def fail(cp):raise RuntimeError('publication')
        with self.assertRaises(RuntimeError):host.step(1,'second',before_publish=fail)
        self.assertEqual(before,host.path.read_bytes());self.assertEqual(host.inspect()['revision'],1)
if __name__=='__main__':unittest.main()
