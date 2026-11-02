"""Opt-in exact native worker, sanitizer, restart and legacy compatibility gate."""
import copy,hashlib,json,math,os
from fractions import Fraction
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import fluid_adapter
from coupling.open_weather_cases import configuration
from coupling.overnight_cases import configuration as legacy_configuration
from coupling.source_replay import Replay
from coupling.journal import Journal
from coupling.live_gust import retire_attempts
from numeric_digest_stream import digest,sealed

class OpenWeatherNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        program=Path(__file__).resolve().parents[1]
        workspace=program.parent.parent if program.parent.name=='_worktrees' else program.parent
        helper=workspace/'bin/verification_retention.py'
        if not helper.is_file():raise ValueError('native weather proof requires CodeWork consumer-check helper before admission')
        sys.path.insert(0,str(helper.parent))
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        cls.workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
        cls.worker=Path(os.environ['WEATHER_WORKER']);cls.sanitizer=Path(os.environ['WEATHER_SANITIZER'])
        cls.root=Path(os.environ['WEATHER_TEST_ROOT']);cls.root.mkdir(parents=True,exist_ok=False)
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(cls.root/'native-attempts')
        cls.config=configuration(cls.workers,'continuous',16)
        pulse=json.loads(Path(os.environ['WEATHER_PULSE']).read_text())
        cls.steps=Replay(pulse,cls.config['receiver']).steps(0,20,Fraction(1,200))
        cls.results={}
        cls.strict=cls.run_native('strict',cls.config['fluid'],None,cls.steps,cls.worker)
    @classmethod
    def run_native(cls,label,config,state,steps,worker):
        result=fluid_adapter.run(config,state,steps,worker)
        fluid_adapter.conservation(config,result)
        data=Journal('lossless_f64_zlib/v1').encode(result)
        path=cls.root/(label+'.checkpoint.zlib');path.write_bytes(data)
        cls.results[label]={'result_digest':result['digest'],'checkpoint_sha256':hashlib.sha256(data).hexdigest(),
            'worker_sha256':result['state']['worker_sha256'],'configuration':config}
        (cls.root/'accepted-results.json').write_text(json.dumps(cls.results,indent=2)+'\n')
        retire_attempts(cls.root/'native-attempts',cls.root/'attempt-receipts')
        return result
    def test_sanitizer_and_packet_state_exact(self):
        c=self.config['fluid']
        sanitizer=self.run_native('sanitizer',c,None,self.steps,self.sanitizer)
        first=self.run_native('first10',c,None,self.steps[:10],self.worker)
        split=self.run_native('split20',c,first['state'],self.steps[10:],self.worker)
        self.assertEqual(digest(self.strict['state']['data']),digest(sanitizer['state']['data']))
        self.assertEqual(self.strict['state']['digest'],split['state']['digest'])
    def test_legacy_native_data_exact(self):
        c=legacy_configuration(self.workers,'gust16')['fluid']
        old=self.run_native('legacy-old',c,None,self.steps,self.workers[2])
        new=self.run_native('legacy-new',c,None,self.steps,self.worker)
        self.assertEqual(digest(old['state']['data']),digest(new['state']['data']))
    def test_zero_driving_exact_quiescence(self):
        c=copy.deepcopy(self.config['fluid'])
        c['open_domain_forcing']['knots'][0]['strength']=0
        zeros=[{'energy_j':[0.]*4096,'smoke_kg':[0.]*4096} for _ in range(20)]
        result=self.run_native('zero-drive',c,None,zeros,self.worker);d=result['state']['data']
        self.assertTrue(all(v==0 for v in d['face_velocity_m_s']))
        self.assertEqual(d['open_domain_forcing_state']['external_work_j'],0)
        self.assertEqual(d['open_domain_forcing_state']['air_in_m3'],0)
        self.assertEqual(d['open_domain_forcing_state']['air_out_m3'],0)
    def test_resealed_work_and_config_tamper_refused(self):
        state=copy.deepcopy(self.strict['state'])
        state['data']['open_domain_forcing_state']['external_work_j']+=1
        with self.assertRaises(ValueError):fluid_adapter.validate(self.config['fluid'],sealed(state))
        c=copy.deepcopy(self.config['fluid']);c['open_domain_forcing']['response_s']=.3
        with self.assertRaises(ValueError):fluid_adapter.validate(c,self.strict['state'])
    def test_old_worker_missing_feature_is_not_accepted(self):
        with self.assertRaises(ValueError):
            fluid_adapter.run(self.config['fluid'],None,[],self.workers[2])
    def test_source_heat_and_mechanical_work_are_separate(self):
        d=self.strict['state']['data']
        expected=math.fsum(math.fsum(s['energy_j']) for s in self.steps)
        self.assertTrue(math.isclose(d['input_energy_j'],expected,rel_tol=1e-12))
        self.assertGreater(d['open_domain_forcing_state']['external_work_j'],0)
        self.assertEqual(d['kinetic_ledger_j'][6],0)
        fluid_adapter.conservation(self.config['fluid'],self.strict)

if __name__=='__main__':unittest.main()
