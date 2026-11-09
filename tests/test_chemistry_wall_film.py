"""Thin-film leveling, gas exchange, and saved inventory qualification."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_reactor import digest
from chemistry_wall_film import WallFilm, example
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class WallFilmTests(unittest.TestCase):
    def test_small_wave_spatial_convergence(self):
        for gravity, tension in ((9.81, 0.), (0., .072), (9.81, .072)):
            errors = []
            for cells in (12, 24, 48):
                config = example(cells)
                config['carrier']['dt_s'] = 2e-8
                h, amplitude = 8e-6, 1e-10
                wave = [math.cos(math.tau * (i + .5) / cells) for i in range(cells)]
                config.update(gravity_m_s2=gravity, tension_n_m=tension, exchange_enabled=False,
                              film_height_m=[h + amplitude * v for v in wave])
                config['deposit']['mass_kg'] = 0.
                film = WallFilm(LIBRARY, config)
                k = math.tau / film.gas.length
                rate = h**3 / (3 * config['viscosity_pa_s']) * (film.rho * gravity * k**2 + tension * k**4)
                derivative = film.derivative(film.state)[film.j:film.j + cells]
                observed = -2 * math.fsum(v * d for v, d in zip(wave, derivative)) / (cells * amplitude * film.mass_per_height)
                errors.append(abs(observed / rate - 1.))
            print('wave errors', gravity, tension, errors)
            self.assertLess(errors[-1], .003)
            self.assertGreater(errors[0] / errors[1], 3.8)
            self.assertGreater(errors[1] / errors[2], 3.8)

    def test_leveling_converts_mechanical_energy_to_heat(self):
        film = WallFilm(LIBRARY, dict(example(), exchange_enabled=False))
        start = film.report()['wall_film']
        film.advance(3000)
        end = film.report()['wall_film']
        released = start['surface_and_gravity_j'] - end['surface_and_gravity_j']
        print('leveling', max(start['height_m']), max(end['height_m']), released, end['viscous_heat_j'])
        self.assertGreater(released, 1e-12)
        self.assertLess(max(end['height_m']), max(start['height_m']))
        self.assertAlmostEqual(end['liquid_kg'], start['liquid_kg'], delta=1e-22)
        self.assertAlmostEqual(end['liquid_sensible_j'] - start['liquid_sensible_j'], released, delta=1e-18)
        self.assertAlmostEqual(end['viscous_heat_j'], released, delta=2e-16)

    def test_phase_exchange_and_restart(self):
        for humidity, key, direction in ((.6, 'evaporated_kg', 1), (1.1, 'condensed_kg', -1)):
            config = example()
            config['carrier']['gas_relative_humidity'] = [humidity] * 24
            film = WallFilm(LIBRARY, config)
            start = film.report()['wall_film']
            film.advance(3000)
            end = film.report()['wall_film']
            self.assertGreater(end[key], 5e-14)
            self.assertLess(max(end['height_m']), max(start['height_m']))
            vapor_gain = direction * (end['gas_vapor_kg'] - start['gas_vapor_kg'])
            self.assertAlmostEqual(vapor_gain, end[key], delta=1e-22)
            self.assertGreater(direction * (start['liquid_temperature_k'][0] - end['liquid_temperature_k'][0]), 0.)
            self.assertLess(abs(end['energy_residual_j']), 1e-16)
            for field in ('water_residual_kg', 'phase_residual_kg', 'dry_air_residual_kg'):
                self.assertLess(abs(end[field]), 1e-21)
            self.assertLess(abs(end['gas_momentum_residual_kg_m_s']), 1e-19)
            saved = packet(film)
            resumed = restore(LIBRARY, saved)
            film.advance(10)
            resumed.advance(10)
            self.assertEqual(packet(resumed), packet(film))
            saved['checkpoint']['payload']['state'][-3] += 1e-10
            saved['checkpoint']['sha256'] = digest(saved['checkpoint']['payload'])
            with self.assertRaisesRegex(ValueError, 'conservation ledger'):
                resumed.restore(saved['checkpoint'])
            self.assertEqual(packet(resumed), packet(film))


if __name__ == '__main__':
    unittest.main()
