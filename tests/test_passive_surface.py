"""Independent geometry and analytic surface-energy verification."""
import json,math,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.surface_observations import mean_temperature,observe,RHO_CP
from coupling.passive_surface import advance,require_burning_admission
from fire_fluid_surface_qualification import response_qualification


class PassiveSurfaceTests(unittest.TestCase):
    def test_linear_volume_average_on_all_axes_and_nested_grids(self):
        lo=[.3125,.421875,.65625];hi=[.6875,.578125,.84375]
        expected=300.+sum(s*(a+b)/2 for s,a,b in zip((2.,3.,4.),lo,hi))
        # Choose boundaries on cell faces: finite-volume cell averages are
        # piecewise constant, so arbitrary subcell linear reconstructions are
        # intentionally not claimed by this sampler.
        for n in (64,128):
            e=[RHO_CP/n**3*(2*(i+.5)/n+3*(j+.5)/n+4*(k+.5)/n) for k in range(n) for j in range(n) for i in range(n)]
            self.assertAlmostEqual(mean_temperature(e,n,lo,hi),expected,places=11)

    def test_constant_field_and_partial_cell_overlap(self):
        n=8;e=[RHO_CP/n**3*7.]*n**3
        self.assertAlmostEqual(mean_temperature(e,n,[.123,.234,.345],[.678,.789,.891]),307.,places=12)
        for f in observe(e,n)['faces'].values():
            self.assertAlmostEqual(f['diagnostic_robin_flux_w_m2'],70.,places=10)
            self.assertEqual(f['actual_insulated_flux_w_m2'],0.)

    def test_no_solid_energy_leaks_into_face_samples(self):
        n=32;e=[0.]*n**3
        for k in range(12,20):
            for j in range(12,20):
                for i in range(12,20):e[(k*n+j)*n+i]=1e6
        self.assertTrue(all(f['gas_temperature_k']==300. for f in observe(e,n)['faces'].values()))

    def test_prescribed_heating_closed_form_balance_and_recovery(self):
        self.assertTrue(response_qualification()['passed'])

    def test_cooling_and_zero_transfer(self):
        state=dict(temperature_k=320.,time_s=0.,stored_j=100.)
        final,r=advance(state,300.,2.,area_m2=1.,capacity_j_k=10.,h_w_m2_k=2.)
        self.assertAlmostEqual(final['temperature_k'],300.+20.*math.exp(-.4),places=12)
        self.assertLess(r['prescribed_gas_transfer_j'],0.)
        self.assertAlmostEqual(r['residual_j'],0.,places=11)
        final,r=advance(state,300.,2.,area_m2=1.,capacity_j_k=10.,h_w_m2_k=0.)
        self.assertEqual(final['temperature_k'],320.);self.assertEqual(r['stored_change_j'],0.)

    def test_invalid_inputs_do_not_mutate_state(self):
        state=dict(temperature_k=300.,time_s=0.,stored_j=0.);before=json.dumps(state)
        for dt in (0.,-1.,float('nan')):
            with self.assertRaises(ValueError):advance(state,310.,dt,area_m2=1.,capacity_j_k=10.,h_w_m2_k=1.)
        self.assertEqual(json.dumps(state),before)

    def test_burning_requires_every_independent_gate(self):
        gates=dict(local_heat_converged=True,gas_surface_flux_converged=True,surface_response_numerics_qualified=True,reciprocal_exchange_qualified=True,material_ignition_qualified=True)
        require_burning_admission(gates)
        for key in gates:
            with self.assertRaises(ValueError):require_burning_admission(dict(gates,**{key:False}))

    def test_geometry_refuses_outside_domain(self):
        with self.assertRaises(ValueError):mean_temperature([0.]*512,8,[-.1,0.,0.],[.1,.1,.1])
        with self.assertRaises(ValueError):mean_temperature([0.]*512,8,[0.,0.],[.1,.1])

if __name__=='__main__':unittest.main()
