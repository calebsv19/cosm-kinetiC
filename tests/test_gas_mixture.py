import math
import json
import tempfile
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import gas_mixture as gas


class MixtureTests(unittest.TestCase):
    def test_reference_coefficients_preserve_serialized_identity(self):
        from fire_lowmach_qualification import save
        from coupling.hotgas import checked
        with tempfile.TemporaryDirectory(prefix='gas-reference-identity-') as scratch:
            save(Path(scratch),'reference',dict(coefficients=gas.DATA))
            artifact=json.loads((Path(scratch)/'reference.json').read_text())
            self.assertEqual(checked(artifact)['coefficients'],json.loads(json.dumps(gas.DATA)))

    def test_nist_published_rounded_cp_tables(self):
        # Independent displayed WebBook tables, not values computed by this module.
        rows = {'N2': [(300,29.12),(500,29.58),(1000,32.69),(1300,34.15)],
                'O2': [(300,29.39),(700,32.98),(1000,34.86)],
                'CO2': [(300,37.22),(1000,54.30),(1200,56.35),(1300,57.14)],
                'H2O': [(500,35.22),(1000,41.27),(1300,44.94)]}
        for name, rows in rows.items():
            for temperature, expected in rows:
                self.assertAlmostEqual(gas.cp_molar(name, temperature), expected, delta=.0051)

    def test_published_enthalpy_differences_across_fit_bands(self):
        # NIST H-H298.15 rounded to .01 kJ/mol. Differences tolerate two roundoffs
        # and small piecewise-fit discontinuities, without extrapolation.
        for name, begin, end, expected in [('N2',300,1300,31.50-.05),
                                         ('O2',300,1000,22.70-.05),
                                         ('CO2',300,1300,50.15-.07),
                                         ('H2O',500,1300,38.94-6.92)]:
            self.assertAlmostEqual(gas.delta_h_molar(name,begin,end)/1000, expected, delta=.015)

    def test_mixture_mass_basis_and_sensible_energy_by_quadrature(self):
        for mixture, start in ((gas.DRY_AIR,300.), (gas.METHANOL_PRODUCTS,500.)):
            p=gas.properties(mixture,1000.)
            mean_mass=sum(n*gas.DATA[k][0] for k,n in mixture.items())/sum(mixture.values())
            self.assertAlmostEqual(p['gas_constant_j_kg_k'],gas.R_UNIVERSAL/mean_mass,places=12)
            # Composite midpoint quadrature split at all coefficient boundaries.
            cuts=sorted({start,1300.,*[edge for k in mixture for row in gas.DATA[k][1]
                      for edge in row[:2] if start<edge<1300.]})
            integral=0.
            for lo,hi in zip(cuts,cuts[1:]):
                step=(hi-lo)/2000
                integral+=step*math.fsum(gas.properties(mixture,lo+(i+.5)*step)['cv_j_kg_k'] for i in range(2000))
            energy=gas.delta_internal(mixture,start,1300.)
            self.assertAlmostEqual(energy,integral,delta=.003)
            self.assertAlmostEqual(gas.temperature_after_energy(mixture,start,energy),1300.,places=9)
            derivative=(gas.delta_internal(mixture,1000.,1000.01)-gas.delta_internal(mixture,1000.,999.99))/.02
            self.assertAlmostEqual(derivative,p['cv_j_kg_k'],delta=1e-6)

    def test_refuses_unsupported_composition_and_extrapolation(self):
        for mixture in ({}, {'CH3OH':1}, {'N2':0}, {'N2':True}, {'N2':math.nan}):
            with self.assertRaises(ValueError): gas.properties(mixture,1000.)
        for temperature in (300.,499.99,1500.01,math.nan,True):
            with self.assertRaises(ValueError): gas.properties(gas.METHANOL_PRODUCTS,temperature)
        with self.assertRaises(ValueError): gas.temperature_after_energy(gas.DRY_AIR,300.,1e8)


if __name__ == '__main__': unittest.main()
