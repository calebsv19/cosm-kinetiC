"""Zero-step initial K and native restore must agree for admitted moving flow."""
import sys,os,copy,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.driven_wind_qualification import configuration
from coupling.kinetic_energy import kinetic
from coupling.open_boundaries import face
from open_atmosphere import run,SCHEMA
class InitialEnergy(unittest.TestCase):
    def request(self,moving):
        workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))
        r=dict(configuration(workers,'abrupt-gust')['fluid'],schema=SCHEMA,state=None,steps=[])
        r['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
        if moving:
            for k in range(8):
                for j in range(9):
                    for i in range(8):r['initial_face_velocity_m_s'][face([8]*3,1,i,j,k,True)]=.05*(k+.5)/8
        return r
    def test_moving_initial_checkpoint_and_exact_restart(self):
        r=self.request(True);worker=os.environ['FIRE_FLUID_OPEN_WORKER'];initial=run(r,worker)
        k=kinetic(r,r['initial_face_velocity_m_s']);self.assertGreater(k,0)
        self.assertEqual(initial['state']['data']['kinetic_ledger_j'][0],initial['state']['data']['kinetic_stored_j'])
        self.assertAlmostEqual(initial['state']['data']['kinetic_ledger_j'][0],k,delta=1e-13*k)
        self.assertEqual(initial['state']['data']['kinetic_ledger_j'][1:],[0.]*9)
        z={'energy_j':[0.]*512,'smoke_kg':[0.]*512}
        full=run(dict(r,steps=[z]*20),worker);resumed=run(dict(r,state=initial['state'],steps=[z]*20),worker)
        self.assertEqual(full['state'],resumed['state'])
    def test_old_worker_failure_reproducer(self):
        with self.assertRaisesRegex(ValueError,'kinetic'):run(self.request(True),os.environ['FIRE_FLUID_BASELINE_WORKER'])
    def test_still_initial_and_evolved_legacy_data_exact(self):
        r=self.request(False);a=run(r,os.environ['FIRE_FLUID_OPEN_WORKER']);b=run(r,os.environ['FIRE_FLUID_BASELINE_WORKER']);self.assertEqual(a['state']['data'],b['state']['data'])
        z={'energy_j':[0.]*512,'smoke_kg':[0.]*512};states=[None,None]
        for count in (160,160):
            for i,key in enumerate(('FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_BASELINE_WORKER')):states[i]=run(dict(r,state=states[i],steps=[z]*count),os.environ[key])['state']
            self.assertEqual(states[0]['data'],states[1]['data'])
if __name__=='__main__':unittest.main()
