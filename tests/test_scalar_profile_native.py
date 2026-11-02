"""Eight bounded native controls for default parity and explicit candidate custody."""
import copy,hashlib,json,os,subprocess,sys,unittest
from fractions import Fraction
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'scripts'),str(Path('/Users/calebsv/Desktop/CodeWork/bin'))]
from coupling.open_weather_cases import configuration
from coupling import fluid_adapter
from coupling.source_replay import Replay
from coupling.scalar_profile import FIELD,PROFILE
from coupling.journal import Journal
from numeric_digest_stream import digest,sealed
class ScalarProfileNative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text());cls.workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')]);cls.worker=Path(os.environ['SCALAR_WORKER']);cls.sanitizer=Path(os.environ['SCALAR_SANITIZER']);cls.root=Path(os.environ['SCALAR_TEST_ROOT']);cls.root.mkdir(exist_ok=False);os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(cls.root/'attempts');cls.c=configuration(cls.workers,'gust',16)['fluid'];cls.candidate=copy.deepcopy(cls.c);cls.candidate[FIELD]=PROFILE
        pulse=json.loads(Path(os.environ['TIMING_PULSE']).read_text());receiver=configuration(cls.workers,'gust',16)['receiver'];cls.steps=Replay(pulse,receiver).steps(0,100,Fraction(1,200));cls.results={};cls.old=Journal('lossless_f64_zlib/v1').decode(Path(os.environ['SCALAR_DEFAULT_DONOR']).read_bytes());cls.new=cls.native('default-new',cls.c,None,cls.steps,cls.worker);cls.strict=cls.native('candidate-strict',cls.candidate,None,cls.steps,cls.worker)
    @classmethod
    def native(cls,label,c,state,steps,worker):
        result=fluid_adapter.run(c,state,steps,worker);fluid_adapter.conservation(c,result);p=cls.root/(label+'.checkpoint.zlib');p.write_bytes(Journal('lossless_f64_zlib/v1').encode(result));cls.results[label]={'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'native_digest':digest(result['state']['data']),'worker_sha256':result['state']['worker_sha256']};(cls.root/'accepted-results.json').write_text(json.dumps(cls.results,indent=2)+'\n');return result
    def test_default_native_data_exact_to_retained_worker(self):
        self.assertEqual(digest(self.old['state']['data']),digest(self.new['state']['data']));self.assertNotIn(FIELD,self.new['state']['data'])
    def test_candidate_sanitizer_and_restart_exact(self):
        san=self.native('candidate-sanitize',self.candidate,None,self.steps,self.sanitizer);first=self.native('candidate-first50',self.candidate,None,self.steps[:50],self.worker);split=self.native('candidate-split100',self.candidate,first['state'],self.steps[50:],self.worker)
        self.assertEqual(digest(san['state']['data']),digest(self.strict['state']['data']));self.assertEqual(split['state']['digest'],self.strict['state']['digest']);self.assertEqual(split['state']['data'][FIELD],PROFILE)
    def test_old_worker_cannot_silently_ignore_candidate(self):
        with self.assertRaises(ValueError):fluid_adapter.run(self.candidate,None,[],Path(self.workers[2]))
    def test_resealed_profile_tamper_and_mode_change_refused(self):
        state=copy.deepcopy(self.strict['state']);state['data'][FIELD]='wrong'
        with self.assertRaises(ValueError):fluid_adapter.validate(self.candidate,sealed(state))
        with self.assertRaises(ValueError):fluid_adapter.validate(self.c,self.strict['state'])
    def test_same_source_and_separate_mechanical_work(self):
        a=self.new['state']['data'];b=self.strict['state']['data'];self.assertEqual(a['input_energy_j'],b['input_energy_j']);self.assertEqual(a['steps'],b['steps']);self.assertEqual(b['scalar_work_cells'],2*a['scalar_work_cells']);self.assertGreater(b['input_energy_j'],0);self.assertGreater(b['open_domain_forcing_state']['external_work_j'],0);self.assertEqual(b['kinetic_ledger_j'][6],0)
if __name__=='__main__':unittest.main()
