import copy
import math
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from passive_atmosphere import atmosphere_worker_path
from open_atmosphere import run,SCHEMA
from surface_sources.growth_fire_v1 import sealed
WORKER=atmosphere_worker_path('open')
def request(nz=8,dt=.01):
    grid=[8,8,nz];n=math.prod(grid);plane=64;zero=[0.]*n
    return {'schema':SCHEMA,'grid':grid,'length_m':[2]*3,'momentum_dt_s':dt,
        'properties':{'density_kg_m3':1,'dynamic_viscosity_pa_s':.02,'heat_capacity_j_kg_k':1000,'reference_temperature_k':300,'conductivity_w_m_k':0,'tracer_diffusivity_m2_s':0},
        'initial_face_velocity_m_s':[0.]*(3*n+plane),'initial_energy_j':zero.copy(),'initial_smoke_kg':zero.copy(),
        'boundary_policy':{'schema':'physics_sim_open_reservoir_boundary/v1','horizontal':'periodic_xy','vertical':'open_bottom_and_top','predictor_velocity':'zero_normal_gradient','pressure_datum_pa':[0,0],'inflow_temperature_k':[300,300],'inflow_smoke_concentration_kg_m3':[0,0],'scalar_diffusion':'zero_normal_flux'},
        'buoyancy':{'enabled':False,'gravity_m_s2':9.81,'expansion_per_k':1/300,'max_temperature_contrast_fraction':.1},'state':None,'steps':[{'energy_j':zero,'smoke_kg':zero}]*10}
class OpenTests(unittest.TestCase):
    def test_uniform_reservoir_and_explicit_per_face_flux(self):
        r=request();n=512;r['initial_face_velocity_m_s'][2*n:]=[1.]*(n+64);r['initial_energy_j']=[3000*(2/8)**3]*n;r['initial_smoke_kg']=[.1*(2/8)**3]*n
        r['boundary_policy']['inflow_temperature_k']=[303]*2;r['boundary_policy']['inflow_smoke_concentration_kg_m3']=[.1]*2
        result=run(r,WORKER)
        self.assertAlmostEqual(result['budgets']['energy_j']['inflow'],1200);self.assertAlmostEqual(result['budgets']['energy_j']['outflow'],1200)
        self.assertAlmostEqual(result['budgets']['smoke_kg']['inflow'],.04);self.assertAlmostEqual(result['budgets']['smoke_kg']['outflow'],.04)
        self.assertAlmostEqual(result['boundary_receipts'][0]['energy_j_inflow']['per_face'][0],1200/64)
    def test_backflow_and_simultaneous_in_out_receipts(self):
        r=request();n=512;r['initial_face_velocity_m_s'][2*n:]=[-1.]*(n+64);r['boundary_policy']['inflow_temperature_k'][1]=303;r['boundary_policy']['inflow_smoke_concentration_kg_m3'][1]=.1;r['steps']=r['steps'][:1]
        result=run(r,WORKER);self.assertAlmostEqual(result['boundary_receipts'][1]['energy_j_inflow']['total'],120);self.assertAlmostEqual(result['budgets']['smoke_kg']['inflow'],.004)
        for q in range(n+64):r['initial_face_velocity_m_s'][2*n+q]=.2*math.sin(2*math.pi*(q%8+.5)/8)
        r['boundary_policy']['inflow_temperature_k']=[303,303];result=run(r,WORKER)
        for receipt in result['boundary_receipts']:
            values=receipt['energy_j_inflow']['per_face'];self.assertTrue(any(v>0 for v in values));self.assertTrue(any(v==0 for v in values));self.assertEqual(receipt['energy_j_outflow']['total'],0)
    def test_native_restart_exact_with_boundary_flux_history(self):
        r=request();n=512;r['initial_face_velocity_m_s'][2*n:]=[.2]*(n+64);r['steps'][0]={'energy_j':[10.]+[0.]*(n-1),'smoke_kg':[.01]+[0.]*(n-1)}
        r['buoyancy']['enabled']=True;whole=run(r,WORKER);first=run(dict(r,steps=r['steps'][:4]),WORKER);resumed=run(dict(r,state=first['state'],steps=r['steps'][4:]),WORKER)
        self.assertEqual(whole['state'],resumed['state']);self.assertEqual(whole['boundary_receipts'],resumed['boundary_receipts'])
    def test_independent_hydrostatic_balance(self):
        r=request();n=512;capacity=1000*(2/8)**3;a=9.81*3/300
        r['initial_energy_j']=[3*capacity]*n;r['boundary_policy']['inflow_temperature_k']=[303]*2;r['buoyancy']['enabled']=True;r['boundary_policy']['pressure_datum_pa']=[-a,a]
        result=run(r,WORKER)
        self.assertLess(max(abs(v) for v in result['fields']['face_velocity_m_s']),1e-11)
        exact=[a*((q//64+.5)*.25-1) for q in range(n)]
        self.assertLess(max(abs(a-b) for a,b in zip(exact,result['fields']['pressure_pa'])),1e-11)
    def test_uniform_thermal_acceleration_and_time_refinement(self):
        a=9.81*3/300
        for nz,dt in ((8,.02),(16,.01)):
            r=request(nz,dt);n=64*nz;capacity=1000*(2/8)**2*(2/nz);r['initial_energy_j']=[3*capacity]*n;r['boundary_policy']['inflow_temperature_k']=[303]*2;r['buoyancy']['enabled']=True
            r['steps']=r['steps'][:round(.1/dt)];result=run(r,WORKER)
            self.assertLess(max(abs(v-a*.1) for v in result['fields']['face_velocity_m_s'][2*n:]),1e-12)
            zero=copy.deepcopy(r);zero['buoyancy']['expansion_per_k']=0;disabled=copy.deepcopy(zero);disabled['buoyancy']['enabled']=False
            self.assertEqual(run(zero,WORKER)['fields']['face_velocity_m_s'],run(disabled,WORKER)['fields']['face_velocity_m_s'])
    def test_spatial_thermal_response_known_acceleration(self):
        r=request();n=512;capacity=1000*(2/8)**3
        theta=[3+math.cos(2*math.pi*(q%8+.5)/8) for q in range(n)]
        r['initial_energy_j']=[t*capacity for t in theta];r['buoyancy']['enabled']=True;r['steps']=r['steps'][:1]
        result=run(r,WORKER);exact=[.01*9.81*t/300 for t in theta]+[.01*9.81*t/300 for t in theta[-64:]]
        self.assertLess(max(abs(a-b) for a,b in zip(exact,result['fields']['face_velocity_m_s'][2*n:])),1e-12)
        self.assertLess(max(abs(v) for v in result['fields']['face_velocity_m_s'][:2*n]),1e-12)
    def test_pulse_translation_refinement_and_outflow_conservation(self):
        errors=[]
        for nz in (16,32):
            r=request(nz,.005);n=64*nz;volume=(2/8)**2*(2/nz);r['initial_face_velocity_m_s'][2*n:]=[1.]*(n+64)
            def concentration(z):return math.cos(math.pi*(z-1.5)/.5)**2 if abs(z-1.5)<.25 else 0.
            r['initial_smoke_kg']=[concentration((q//64+.5)*2/nz)*volume for q in range(n)];r['steps']=[r['steps'][0]]*50
            result=run(r,WORKER);exact=[concentration((q//64+.5)*2/nz-.25)*volume for q in range(n)]
            errors.append(math.fsum(abs(a-b) for a,b in zip(exact,result['fields']['smoke_kg'])))
            self.assertAlmostEqual(result['budgets']['smoke_kg']['stored']+result['budgets']['smoke_kg']['outflow'],math.fsum(r['initial_smoke_kg']),places=12)
        self.assertLess(errors[1],.8*errors[0])
    def test_policy_contrast_configuration_and_state_rejections(self):
        r=request();bad=copy.deepcopy(r);bad['boundary_policy']['vertical']='ground_and_open_top'
        with self.assertRaises(ValueError):run(bad,WORKER)
        bad=copy.deepcopy(r);bad['buoyancy']['enabled']=True;bad['initial_energy_j'][0]=1e6
        with self.assertRaises(ValueError):run(bad,WORKER)
        bad=copy.deepcopy(r);bad['buoyancy']['enabled']=True;bad['boundary_policy']['inflow_temperature_k'][0]=400
        with self.assertRaises(ValueError):run(bad,WORKER)
        good=run(r,WORKER);bad=copy.deepcopy(good['state']);bad['data']['boundary_fluxes'][0]=1
        with self.assertRaises(ValueError):run(dict(r,state=bad),WORKER)
        with self.assertRaises(ValueError):run(dict(r,state=sealed(bad)),WORKER)
        bad=copy.deepcopy(r);bad['boundary_policy']['pressure_datum_pa'][0]=1
        with self.assertRaises(ValueError):run(dict(bad,state=good['state']),WORKER)
if __name__=='__main__':unittest.main()
