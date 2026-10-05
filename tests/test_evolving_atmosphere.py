import copy
import math
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from evolving_atmosphere import run,SCHEMA
from surface_sources.growth_fire_v1 import sealed
WORKER=ROOT/'build/evolving-atmosphere/physics_sim_atmosphere_worker'
def request():
    n=512;v=[.2+.05*math.cos(2*math.pi*((q//8)%8+.5)/8) for q in range(n)]+[.05]*n+[.1]*n
    j=[0.]*n;j[0]=100;kg=[0.]*n;kg[0]=.01
    return {'schema':SCHEMA,'grid':[8]*3,'length_m':[2]*3,'properties':{'density_kg_m3':1.225,'dynamic_viscosity_pa_s':.0245,'heat_capacity_j_kg_k':1000,'reference_temperature_k':300,'conductivity_w_m_k':.1,'tracer_diffusivity_m2_s':.001},
        'momentum_dt_s':.01,'initial_face_velocity_m_s':v,'state':None,'steps':[{'energy_j':j,'smoke_kg':kg} for _ in range(10)]}
class EvolvingTests(unittest.TestCase):
    def test_native_restart_exact_and_evolution(self):
        r=request();velocity=[]
        for axis in range(3):
            for q in range(512):
                x,y,z=[2*math.pi*(c+.5)/8 for c in (q%8,(q//8)%8,q//64)]
                velocity.append(.02*((math.sin(z)+math.cos(y)) if axis==0 else (math.sin(x)+math.cos(z)) if axis==1 else (math.sin(y)+math.cos(x))))
        r['initial_face_velocity_m_s']=velocity
        whole=run(r,WORKER);first=run(dict(r,steps=r['steps'][:4]),WORKER)
        self.assertGreater(max(abs(p) for p in whole['fields']['pressure_pa']),1e-6)
        resumed=run(dict(r,state=first['state'],steps=r['steps'][4:]),WORKER)
        self.assertEqual(whole['state'],resumed['state'])
        for key in ('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','temperature_k'):self.assertEqual(whole['fields'][key],resumed['fields'][key])
        self.assertNotEqual(r['initial_face_velocity_m_s'],whole['fields']['face_velocity_m_s'])
        self.assertAlmostEqual(whole['budgets']['energy_j']['stored'],1000)
        self.assertLess(whole['fields']['max_divergence_s_inv'],1e-8)
    def test_independent_discrete_shear_decay(self):
        r=request();r['initial_face_velocity_m_s'][512:1024]=[0.]*512
        r['steps']=[{'energy_j':[0.]*512,'smoke_kg':[0.]*512}]*10
        result=run(r,WORKER)
        rate=.02*4*math.sin(math.pi/8)**2/(2/8)**2
        older=1.;amplitude=1/(1+.01*rate)
        for _ in range(1,10):older,amplitude=amplitude,(2*amplitude-.5*older)/(1.5+.01*rate)
        exact=[.2+.05*amplitude*math.cos(2*math.pi*((q//8)%8+.5)/8) for q in range(512)]
        self.assertLess(max(abs(a-b) for a,b in zip(exact,result['fields']['face_velocity_m_s'])),1e-10)
    def test_uniform_and_zero_source_controls(self):
        r=request();r['initial_face_velocity_m_s']=[.2]*512+[0.]*1024;r['steps']=[{'energy_j':[0.]*512,'smoke_kg':[0.]*512}]*5
        result=run(r,WORKER);self.assertLess(max(abs(a-b) for a,b in zip(result['fields']['face_velocity_m_s'],r['initial_face_velocity_m_s'])),1e-12);self.assertEqual(result['budgets']['smoke_kg']['stored'],0)
    def test_malformed_state_configuration_and_divergence_reject(self):
        r=request();accepted=run(dict(r,steps=r['steps'][:2]),WORKER)
        changed=copy.deepcopy(accepted['state']);changed['data']['steps']=8
        with self.assertRaises(ValueError):run(dict(r,state=changed),WORKER)
        with self.assertRaises(ValueError):run(dict(r,state=sealed(changed)),WORKER)
        props=dict(r['properties'],dynamic_viscosity_pa_s=.05)
        with self.assertRaises(ValueError):run(dict(r,state=accepted['state'],properties=props),WORKER)
        r['initial_face_velocity_m_s'][0]=1
        with self.assertRaises(ValueError):run(r,WORKER)
if __name__=='__main__':unittest.main()
