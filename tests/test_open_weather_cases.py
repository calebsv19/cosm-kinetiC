"""Physical and scheduling invariants across authored open-weather recipes."""
from fractions import Fraction
import json,os
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.open_weather_cases import configuration,PROFILES
from coupling.open_weather import command

class OpenWeatherCasesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        cls.workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
    def test_fixed_source_clocks_cutoff_and_object(self):
        original=configuration(self.workers,'continuous',16)
        for n in (16,32):
            for profile in PROFILES:
                c=configuration(self.workers,profile,n);f=c['fluid']
                self.assertEqual(c['source'],original['source'])
                self.assertEqual(c['periods'],[[1,50],[1,200],[1,10]])
                self.assertEqual(Fraction(*c['periods'][2])*c['windows'],Fraction(16,5))
                self.assertEqual(Fraction(*c['periods'][2])*c['source_delivery']['active_windows'],Fraction(8,5))
                self.assertEqual(f['inert_object'],original['fluid']['inert_object'])
                self.assertEqual(f['solid_box'],original['fluid']['solid_box'])
                self.assertEqual(f['properties'],original['fluid']['properties'])
                self.assertEqual(c['receiver']['injection_depth_m'],original['receiver']['injection_depth_m'])
                self.assertNotIn('wind_forcing',f)
    def test_release_fade_and_reversal_commands(self):
        f=configuration(self.workers,'gust')['fluid']
        self.assertEqual(command(f,79),(.5,0));self.assertEqual(command(f,80),(.5,1))
        self.assertEqual(command(f,240),(.5,0))
        f=configuration(self.workers,'fade')['fluid']
        self.assertEqual(command(f,100),(.5,.5));self.assertEqual(command(f,280),(.5,.5))
        self.assertEqual(command(f,320),(.5,0))
        f=configuration(self.workers,'reversal')['fluid']
        self.assertEqual(command(f,240),(-.5,1));self.assertEqual(command(f,480),(-.5,0))
    def test_unbounded_recipe_refuses(self):
        with self.assertRaises(ValueError):configuration(self.workers,'unknown')
        with self.assertRaises(ValueError):configuration(self.workers,'gust',64)

if __name__=='__main__':unittest.main()
