"""Explicit momentum policy, exact native restart and independently checked KE."""
import copy,os,sys,tempfile,unittest,zlib,sqlite3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.driven_wind_qualification import configuration
from coupling.kinetic_energy import kinetic,validate as energy_valid
from coupling.journal import Journal
from coupling.framework import Framework
from open_atmosphere import run,validate,SCHEMA
from numeric_digest_stream import sealed
class Quiescent(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
        self.config=configuration(self.workers,'abrupt-gust')
    def request(self):
        f=self.config['fluid'];f['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
        zero={'energy_j':[0.]*512,'smoke_kg':[0.]*512}
        return dict(f,schema=SCHEMA,state=None,steps=[zero]*240)
    def test_native_restart_and_ledger_tamper(self):
        r=self.request();a=run(r,self.workers[2]);b=run(dict(r,steps=r['steps'][:161]),self.workers[2]);c=run(dict(r,state=b['state'],steps=r['steps'][161:]),self.workers[2])
        self.assertEqual(a['state'],c['state']);energy_valid(r,a['state']['data'])
        self.assertGreater(a['state']['data']['kinetic_ledger_j'][6],0)
        for field in ('kinetic_stored_j','kinetic_ledger_j'):
            bad=copy.deepcopy(a['state'])
            if field=='kinetic_stored_j':bad['data'][field]+=.01
            else:bad['data'][field][2]+=.01
            bad=sealed(bad)
            with self.assertRaises(ValueError):validate(dict(r,state=bad,steps=[]))
    def test_legacy_default_and_bad_policy(self):
        r=self.request();r['steps']=r['steps'][:10];r['wind_forcing'].pop('momentum_backflow')
        a=run(r,self.workers[2]);self.assertNotIn('kinetic_ledger_j',a['state']['data'])
        r['wind_forcing']['momentum_backflow']='unknown'
        with self.assertRaises(ValueError):validate(r)
    def test_compressed_joint_restart_replay_stale_and_failed_publish(self):
        c=self.config;c['journal_codec']=os.environ.get('FIRE_FLUID_TEST_JOURNAL_CODEC','lossless_zlib/v1');c['windows']=3
        if os.environ.get('FIRE_FLUID_TEST_PACKETS'):c['native_packet_steps']=5;c['fluid']['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
        f=Framework(c,Path(self.temp.name)/'run');initial=f.initialize()
        a=f.step(0,'one');self.assertEqual(f.step(0,'one'),a)
        with self.assertRaisesRegex(ValueError,'stale'):f.step(0,'two')
        def fail(cp):raise RuntimeError('injected publication failure')
        with self.assertRaises(RuntimeError):f.step(1,'two',before_publish=fail)
        self.assertEqual(f.inspect()['revision'],1)
        f=Framework(c,Path(self.temp.name)/'run');self.assertEqual(f.inspect()['revision'],1)
        f.step(1,'two');f.step(2,'three');self.assertEqual(Framework(c,Path(self.temp.name)/'run').inspect()['revision'],3)
        with sqlite3.connect(f.path) as db:db.execute('UPDATE checkpoints SET payload=? WHERE revision=1',(zlib.compress(b'{}'),))
        with self.assertRaises((ValueError,KeyError)):f.inspect()
    def test_codec_exact_and_refusal(self):
        j=Journal('lossless_zlib/v1');value={'signed':[-0.,1e-300,1.23456789012345], 'i':2**53}
        raw=j.encode(value);self.assertEqual(j.decode(raw),value)
        for bad in (raw[:-1],raw+b'junk',b'nope'):
            with self.assertRaises((ValueError,zlib.error)):j.decode(bad)
        with self.assertRaises(ValueError):Journal('lossy')
        with self.assertRaises(ValueError):Journal().decode(raw)
if __name__=='__main__':unittest.main()
