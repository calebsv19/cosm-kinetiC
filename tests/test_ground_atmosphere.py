"""Independent ground hydrostatics, no-leak scalar budgets and exact restart."""
import copy, math, unittest
from test_open_atmosphere import request, WORKER
from open_atmosphere import run
from surface_sources.growth_fire_v1 import sealed

def ground():
    r=request();r['boundary_policy'].update(vertical='solid_bottom_open_top',predictor_velocity='no_slip_bottom_zero_gradient_top');return r
class GroundTests(unittest.TestCase):
    def test_uniform_buoyancy_balanced_by_ground_pressure(self):
        r=ground();r['buoyancy']['enabled']=True;capacity=1000*.25**3;r['initial_energy_j']=[3*capacity]*512
        x=run(r,WORKER);a=9.81*3/300
        self.assertLess(max(map(abs,x['fields']['face_velocity_m_s'])),1e-11)
        self.assertLess(max(abs(p-a*((q//64+.5)*.25-2)) for q,p in enumerate(x['fields']['pressure_pa'])),1e-11)
    def test_source_at_floor_no_leak_diffusion_buoyancy_and_restart(self):
        r=ground();r['properties']['tracer_diffusivity_m2_s']=.002;r['buoyancy']['enabled']=True
        r['steps'][0]={'energy_j':[10.]+[0.]*511,'smoke_kg':[.001]+[0.]*511}
        x=run(r,WORKER);first=run(dict(r,steps=r['steps'][:4]),WORKER)
        y=run(dict(r,state=first['state'],steps=r['steps'][4:]),WORKER);self.assertEqual(x['state'],y['state'])
        self.assertTrue(any(v>0 for v in x['fields']['face_velocity_m_s'][2*512+64:]))
        self.assertEqual(x['fields']['face_velocity_m_s'][2*512:2*512+64],[0.]*64)
        for key in ('energy_j','smoke_kg'):
            self.assertEqual(x['boundary_receipts'][0][key+'_inflow']['total'],0)
            self.assertEqual(x['boundary_receipts'][0][key+'_outflow']['total'],0)
            b=x['budgets'][key];self.assertAlmostEqual(b['stored']+b['outflow']-b['inflow'],b['input'],places=12)
    def test_bottom_velocity_and_forged_bottom_flux_rejected(self):
        r=ground();r['initial_face_velocity_m_s'][1024]=.1
        with self.assertRaises(ValueError):run(r,WORKER)
        r=ground();good=run(r,WORKER);bad=copy.deepcopy(good['state']);bad['data']['boundary_fluxes'][0]=1
        with self.assertRaises(ValueError):run(dict(r,state=sealed(bad)),WORKER)
    def test_tangential_wall_dissipates_uniform_flow(self):
        r=ground();r['initial_face_velocity_m_s'][:512]=[.1]*512;x=run(r,WORKER)
        self.assertLess(x['fields']['face_velocity_m_s'][0],x['fields']['face_velocity_m_s'][7*64])
        self.assertLess(max(x['fields']['face_velocity_m_s'][:512]),.1000000001)
if __name__=='__main__':unittest.main()
