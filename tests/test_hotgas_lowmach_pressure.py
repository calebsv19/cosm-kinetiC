import copy
import json
import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import vortex_reference as vortex,lowmach_pressure_cases as controls,lowmach_cases as old,hotgas_domain as domain,buoyancy_reference as buoyancy
from coupling.hotgas_domain_cases import config,hydrostatic
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']


class LowMachPressureTests(unittest.TestCase):
    def test_independent_radial_force_balance(self):
        # Finite difference the prescribed pressure independently of native flux.
        for radius in (.07,.15,.25,.33,.45):
            h=1e-6
            def pressure(r):
                u=vortex.profile(.5+r,.5,.01)
                return .4*(u[4]-.5*(u[1]**2+u[2]**2))
            derivative=(pressure(radius+h)-pressure(radius-h))/(2*h)
            u=vortex.profile(.5+radius,.5,.01)
            self.assertAlmostEqual(derivative,(u[1]**2+u[2]**2)/radius,delta=1e-6)
        for boundary in (.2,.4):
            left=vortex.profile(.5+boundary-1e-8,.5,.01)
            right=vortex.profile(.5+boundary+1e-8,.5,.01)
            self.assertLess(max(abs(a-b) for a,b in zip(left,right)),2e-6)

    def test_stationary_vortex_low_mach_scaling(self):
        rows=[vortex.case(WORKER,'lhllc_muscl',mach=m) for m in (.01,.001)]
        for r in rows:
            self.assertLess(r['relative_velocity_l1'],.1)
            self.assertLess(r['relative_pressure_shape_l1'],.15)
            self.assertGreater(r['kinetic_energy_ratio'],.98)
        self.assertLess(rows[1]['relative_velocity_l1']/rows[0]['relative_velocity_l1'],1.2)
        self.assertLess(abs(rows[0]['normalized_pressure_range']/rows[1]['normalized_pressure_range']-1),.1)
        row=rows[0];source=[0.]*len(row['initial']['conserved']);c=row['config'];rate=row['rate'];end=row['final']['tick']
        middle,_=domain.advance(c,WORKER,row['initial'],source,end//3,rate)
        resumed,_=domain.advance(c,WORKER,json.loads(json.dumps(middle)),source,end,rate)
        self.assertEqual(row['final'],resumed)

    def test_prepared_buoyancy_pressure_reference(self):
        row=buoyancy.case(WORKER,'lhllc_muscl',16,prepared=True)
        self.assertLess(row['relative_velocity_l1'],.1)
        self.assertLess(row['relative_pressure_perturbation_l1'],.1)

    def test_weak_pulse_characteristics(self):
        rows={b:old.pressure_pulse(WORKER,b,'lhllc_muscl')['left_reflection_l1_ratio']
              for b in ('periodic','slip_wall','reservoir')}
        self.assertLess(max(rows['periodic'],rows['reservoir']),.01)
        self.assertGreater(rows['slip_wall'],.95)
        self.assertLess(rows['slip_wall'],1.05)

    def test_acoustic_preservation_and_refinement(self):
        errors=[old.acoustic(WORKER,'lhllc_muscl',n)['relative_pressure_l1_error'] for n in (16,32,64)]
        self.assertLess(errors[-1],.004)
        for a,b in zip(errors,errors[1:]):self.assertGreater(a/b,2.5)

    def test_broadband_stability_at_physical_acoustic_cfl(self):
        for gravity in (False,True):
            r=controls.noise(WORKER,gravity)
            self.assertLess(r['acoustic_energy_ratio'],1.05)

    def test_contact_equilibrium_restart_and_policy_refusal(self):
        self.assertLess(old.stationary_contact(WORKER,'lhllc_muscl')['max_density_error_kg_m3'],1e-12)
        c=config();c['domain']['flux']='lhllc_muscl';u=hydrostatic(c);source=[0.]*len(u);rate=(1,1000000)
        initial=domain.initial(c,WORKER,u,source,rate)
        final,_=domain.advance(c,WORKER,initial,source,300,rate)
        self.assertLess(max(abs(a-b) for a,b in zip(u,final['conserved'])),1e-7)
        middle,_=domain.advance(c,WORKER,initial,source,71,rate)
        resumed,_=domain.advance(c,WORKER,json.loads(json.dumps(middle)),source,300,rate)
        self.assertEqual(final,resumed)
        other=copy.deepcopy(c);other['domain']['flux']='hllc_muscl'
        with self.assertRaises(ValueError):domain.advance(other,WORKER,middle,source,300,rate)

    def test_rk3_boundary_and_conduction_ledgers(self):
        import test_hotgas_domain as retained
        original=retained.config
        def selected(*args,**kwargs):
            c=original(*args,**kwargs);c['domain']['flux']='lhllc_muscl';return c
        retained.config=selected
        try:
            for name in ('test_closed_slip_walls_zero_mass_and_energy_leak',
                         'test_uniform_reservoir_throughflow_known_face_budgets',
                         'test_multiaxis_hydrostatic_open_reservoir'):
                retained.DomainTests(name).debug()
            c=selected(gravity=(0.,0.,0.),conductivity=.026)
            c['domain']['faces'][4]=retained.face(wall_temperature=300.)
            u=retained.conserved(1.,400.)*math.prod(c['gas']['grid'])
            _,_,r=retained.run(c,u,100)
            self.assertGreater(r['boundary_budget']['conducted_outward_j'][4],0)
            self.assertLess(max(abs(v) for v in r['conservation_residual']),1e-8)
        finally:retained.config=original


if __name__=='__main__':unittest.main()
