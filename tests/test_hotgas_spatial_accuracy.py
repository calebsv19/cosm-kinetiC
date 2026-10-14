import copy
import json
import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import hotgas_domain as domain, lowmach_cases as cases, buoyancy_reference as reference
from coupling.hotgas_domain_cases import config,hydrostatic
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']


class SpatialAccuracyTests(unittest.TestCase):
    def test_rk2_boundary_conservation(self):
        import test_hotgas_domain as retained
        original=retained.config
        def selected(*args,**kwargs):
            c=original(*args,**kwargs);c['domain']['flux']='hllc_muscl';return c
        retained.config=selected
        try:
            for name in ('test_closed_slip_walls_zero_mass_and_energy_leak',
                         'test_uniform_reservoir_throughflow_known_face_budgets',
                         'test_multiaxis_hydrostatic_open_reservoir'):
                retained.DomainTests(name).debug()
            c=selected(gravity=(0.,0.,0.),conductivity=.026)
            c['domain']['faces'][4]=retained.face(wall_temperature=300.)
            u=retained.conserved(1.,400.)*math.prod(c['gas']['grid'])
            _,_,receipt=retained.run(c,u,100)
            self.assertGreater(receipt['boundary_budget']['conducted_outward_j'][4],0)
            self.assertLess(max(abs(v) for v in receipt['conservation_residual']),1e-8)
        finally: retained.config=original

    def test_independent_fourier_ode_reference(self):
        # Integrate the three modal ODEs directly, without using the analytic
        # acoustic/solenoidal split used by the reference evaluator.
        kx=2*math.pi/.1;kz=math.pi/.1;a2=1.4*287*300;B=.00981
        def rhs(v):
            x,z,p=v
            return (kx*p,kz*p+B,-a2*(kx*x+kz*z))
        state=[0.,0.,0.];dt=2e-7
        for _ in range(10000):
            k1=rhs(state);k2=rhs([v+dt*k/2 for v,k in zip(state,k1)])
            k3=rhs([v+dt*k/2 for v,k in zip(state,k2)])
            k4=rhs([v+dt*k for v,k in zip(state,k3)])
            state=[v+dt*(a+2*b+2*c+d)/6 for v,a,b,c,d in zip(state,k1,k2,k3,k4)]
        exact=reference.coefficients(.1,300,1.4,287,B,.002)
        for actual,key in zip(state,('horizontal','vertical','pressure_over_density')):
            self.assertAlmostEqual(actual,exact[key],delta=1e-12)

    def test_smooth_acoustic_second_order_refinement(self):
        errors=[cases.acoustic(WORKER,'hllc_muscl',n)['relative_pressure_l1_error'] for n in (16,32,64)]
        self.assertLess(errors[-1],.003)
        for coarse,fine in zip(errors,errors[1:]): self.assertGreater(coarse/fine,2.5)

    def test_contact_equilibrium_and_exact_restart(self):
        self.assertLess(cases.stationary_contact(WORKER,'hllc_muscl')['max_density_error_kg_m3'],1e-12)
        c=config();c['domain']['flux']='hllc_muscl';u=hydrostatic(c);source=[0.]*len(u);rate=(1,1000000)
        initial=domain.initial(c,WORKER,u,source,rate)
        final,_=domain.advance(c,WORKER,initial,source,300,rate)
        self.assertLess(max(abs(a-b) for a,b in zip(u,final['conserved'])),1e-7)
        middle,_=domain.advance(c,WORKER,initial,source,71,rate)
        recovered,_=domain.advance(c,WORKER,json.loads(json.dumps(middle)),source,300,rate)
        self.assertEqual(final,recovered)
        altered=copy.deepcopy(c);altered['domain']['flux']='hllc'
        with self.assertRaises(ValueError): domain.advance(altered,WORKER,middle,source,300,rate)

    def test_small_buoyancy_velocity_refinement(self):
        # Selected smooth linear limit, not finite-contrast plume calibration.
        # Sanitizer instrumentation exceeds the native 60-second ceiling on
        # the largest case. Keep the full mesh accuracy gate on strict builds;
        # instrument smaller resolved cases rather than widening that ceiling.
        instrumented=os.environ.get('HOTGAS_SPATIAL_SANITIZED')=='1'
        rows=[reference.case(WORKER,'hllc_muscl',n) for n in ((8,16) if instrumented else (16,32))]
        self.assertLess(rows[-1]['relative_velocity_l1'],.25 if instrumented else .1)
        self.assertGreater(rows[0]['relative_velocity_l1']/rows[1]['relative_velocity_l1'],2)
        self.assertLess(rows[-1]['relative_pressure_perturbation_l1'],rows[0]['relative_pressure_perturbation_l1'])


if __name__=='__main__': unittest.main()
