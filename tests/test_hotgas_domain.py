import copy
import json
import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import hotgas, hotgas_domain as domain
from coupling.hotgas_domain_cases import config,face,conserved,hydrostatic,temperature_profile
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']
RATE=(1,1000000)


def run(c,u,count=10,source=None,rate=RATE):
    source=source if source is not None else [0.]*len(u)
    initial=domain.initial(c,WORKER,u,source,rate)
    final,receipt=domain.advance(c,WORKER,initial,source,count,rate)
    return initial,final,receipt


class DomainTests(unittest.TestCase):
    def test_closed_slip_walls_zero_mass_and_energy_leak(self):
        c=config(gravity=(0.,0.,0.));u=conserved(1.,300.,(5.,-3.,2.))*math.prod(c['gas']['grid'])
        _,_,r=run(c,u,100)
        for f in r['boundary_budget']['outward']:
            self.assertEqual(f[0],0.);self.assertEqual(f[4],0.)
        self.assertLess(max(abs(v) for v in r['conservation_residual']),1e-8)

    def test_uniform_reservoir_throughflow_known_face_budgets(self):
        c=config(gravity=(0.,0.,0.));v=(5.,0.,0.);rho=101325/(287*300)
        c['domain']['faces']=[face('reservoir',velocity=v) for _ in range(6)]
        u=conserved(rho,300.,v)*math.prod(c['gas']['grid']);_,f,r=run(c,u,100)
        self.assertLess(max(abs(a-b) for a,b in zip(u,f['conserved'])),1e-9)
        mass=rho*5*.1*.1*100e-6
        enthalpy=rho*287*300/.4+.5*rho*25+101325
        self.assertAlmostEqual(r['boundary_budget']['outward'][0][0],-mass,places=14)
        self.assertAlmostEqual(r['boundary_budget']['outward'][1][0],mass,places=14)
        self.assertAlmostEqual(r['boundary_budget']['outward'][1][4],enthalpy*5*.01*100e-6,places=10)

    def test_reservoir_imbalance_closes_boundary_ledger(self):
        c=config(gravity=(0.,0.,0.));c['domain']['faces'][0]=face('reservoir',velocity=(1.,0.,0.))
        rho=101325/(287*300);u=conserved(rho,300.)*math.prod(c['gas']['grid'])
        _,_,r=run(c,u,1)
        self.assertAlmostEqual(r['boundary_budget']['outward'][0][0],-.5*rho*.01*1e-6,places=15)
        self.assertLess(max(abs(v) for v in r['conservation_residual']),1e-10)

    def test_hydrostatic_closed_and_open_equilibrium(self):
        for top in ('slip_wall','reservoir'):
            c=config();c['domain']['faces'][5]=face(top);u=hydrostatic(c)
            _,f,r=run(c,u,1000)
            self.assertLess(max(abs(v) for a in (1,2,3) for v in f['conserved'][a::5]),1e-9)
            self.assertLess(max(abs(a-b) for a,b in zip(u,f['conserved'])),1e-7)
            self.assertLess(abs(r['gas_plus_potential_residual_j']),1e-8)

    def test_hot_parcel_initial_buoyant_response_and_potential_energy(self):
        c=config(grid=(4,4,32),length=(.1,.1,1.));c['domain']['faces'][5]=face('reservoir')
        u=hydrostatic(c);plane=16;indices=[]
        for z in range(12,20):
            for q in range(z*plane,(z+1)*plane):
                u[5*q]*=1/3 # 900 K at unchanged equilibrium pressure
                indices.append(q)
        _,f,r=run(c,u,1)
        average=sum(f['conserved'][5*q+3]/f['conserved'][5*q] for q in indices)/len(indices)
        self.assertGreater(average,0.)
        self.assertLess(abs(r['gas_plus_potential_residual_j']),1e-8)
        # Interior parcel cells have the initial continuum buoyant acceleration.
        q=16*plane
        self.assertAlmostEqual(f['conserved'][5*q+3]/f['conserved'][5*q],19.62e-6,delta=1e-9)

    def test_multiaxis_hydrostatic_open_reservoir(self):
        c=config(grid=(4,4,8),length=(10.,10.,10.),gravity=(2.,-3.,-9.81))
        c['domain']['faces']=[face('reservoir') for _ in range(6)]
        u=hydrostatic(c);_,f,r=run(c,u,1000)
        self.assertLess(max(abs(a-b) for a,b in zip(u,f['conserved'])),1e-7)
        self.assertLess(max(abs(v) for a in (1,2,3) for v in f['conserved'][a::5]),1e-9)
        self.assertLess(abs(r['gas_plus_potential_residual_j']),1e-4)

    def test_conductive_wall_transfer_is_conservative_and_signed(self):
        c=config(gravity=(0.,0.,0.),conductivity=.026)
        c['domain']['faces'][4]=face(wall_temperature=300.)
        u=conserved(1.,400.)*math.prod(c['gas']['grid'])
        _,_,r=run(c,u,1)
        heat=.026*100/(.5*.1/16)*.01*1e-6
        self.assertAlmostEqual(r['boundary_budget']['conducted_outward_j'][4],heat,places=14)
        self.assertAlmostEqual(r['totals_before'][4]-r['totals_after'][4],heat,places=10)
        c['domain']['faces'][4]['wall_temperature_k']=500.
        _,_,r=run(c,u,1)
        self.assertAlmostEqual(r['boundary_budget']['conducted_outward_j'][4],-heat,places=14)

    def test_manufactured_surface_and_gauge_plane_conduction(self):
        c=config(grid=(4,4,32),length=(.032,.032,.032),gravity=(0.,0.,0.),conductivity=.026)
        c['domain']['faces'][4]=face(wall_temperature=300.)
        u=temperature_profile(c);source=[0.]*len(u)
        s=domain.initial(c,WORKER,u,source,RATE)
        observation=domain.observe(c,WORKER,s,source,RATE,[0.,.013])
        self.assertFalse(observation['reciprocal_thermal_exchange_admissible'])
        self.assertIsNone(observation['radiative_flux_w_m2'])
        for expected,sample in zip((300.,313.),observation['native_samples']):
            self.assertLess(max(abs(t-expected) for t in sample['temperature_k']),1e-10)
            self.assertLess(max(abs(q-26.) for q in sample['conducted_downward_w_m2']),1e-10)
            self.assertTrue(all(v==0. for v in sample['advected_upward_w_m2']))

    def test_curved_profile_surface_refinement_and_resolved_gauge(self):
        errors=[]
        for nz in (8,16,32):
            c=config(grid=(4,4,nz),length=(.032,.032,.032),gravity=(0.,0.,0.),conductivity=.026)
            c['domain']['faces'][4]=face(wall_temperature=300.)
            u=temperature_profile(c,curvature=20000.);source=[0.]*len(u)
            s=domain.initial(c,WORKER,u,source,RATE)
            samples=domain.observe(c,WORKER,s,source,RATE,[0.,.013])['native_samples']
            errors.append(abs(samples[0]['conducted_downward_w_m2'][0]-26.))
            self.assertAlmostEqual(samples[1]['conducted_downward_w_m2'][0],.026*(1000+40000*.013),places=9)
        self.assertAlmostEqual(errors[0]/errors[1],2.,places=9)
        self.assertAlmostEqual(errors[1]/errors[2],2.,places=9)

    def test_restart_controls_and_transactional_refusals(self):
        c=config();u=hydrostatic(c);source=[0.]*len(u)
        s=domain.initial(c,WORKER,u,source,RATE)
        whole,_=domain.advance(c,WORKER,s,source,20,RATE)
        middle,_=domain.advance(c,WORKER,s,source,7,RATE)
        split,_=domain.advance(c,WORKER,json.loads(json.dumps(middle)),source,20,RATE)
        self.assertEqual(whole,split)
        altered=copy.deepcopy(c);altered['domain']['faces'][5]=face('reservoir')
        with self.assertRaises(ValueError): domain.advance(altered,WORKER,middle,source,20,RATE)
        before=copy.deepcopy(s)
        bad=domain.initial(c,WORKER,u,source,(1,1))
        bad_before=copy.deepcopy(bad)
        with self.assertRaises(ValueError): domain.advance(c,WORKER,bad,source,1,(1,1))
        self.assertEqual(bad,bad_before)
        self.assertEqual(s,before)
        c['domain']['faces'][4]=face('periodic')
        with self.assertRaises(ValueError): domain.initial(c,WORKER,u,source,RATE)


if __name__=='__main__': unittest.main()
