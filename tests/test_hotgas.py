import copy
import json
import math
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import hotgas

WORKER = os.environ['HOTGAS_WORKER']


def config(nx=4):
    return dict(grid=[nx, 4, 4], length_m=[1., 1., 1.], gamma=1.4, gas_constant_j_kg_k=287.)


def cell(rho=1., velocity=0., pressure=101325.):
    return [rho, rho*velocity, 0., 0., pressure/.4+.5*rho*velocity**2]


def contact(nx, velocity=100.):
    c = config(nx)
    lo, hi = 101325/(287*1290), 101325/(287*300)
    # Exact finite-volume cell average of periodic sine, rather than point data.
    amplitude = (hi-lo)/2
    sinc = math.sin(math.pi/nx)/(math.pi/nx)
    u = []
    for q in range(nx*16):
        rho = (hi+lo)/2+amplitude*sinc*math.sin(2*math.pi*((q % nx)+.5)/nx)
        u += cell(rho, velocity)
    return c, u


def convergence():
    result = []
    for nx in (32, 64, 128):
        c, u = contact(nx); source = [0.]*len(u)
        steps = 2*nx; rate = (1, 4000*nx)  # matched physical duration 0.0005 s
        initial = hotgas.initial(c, WORKER, u, source, rate)
        final, receipt = hotgas.advance(c, WORKER, initial, source, steps, rate)
        lo, hi = 101325/(287*1290), 101325/(287*300)
        sinc = math.sin(math.pi/nx)/(math.pi/nx)
        error = math.fsum(abs(final['conserved'][5*q] - ((hi+lo)/2+(hi-lo)/2*sinc*
            math.sin(2*math.pi*((q % nx)+.5)/nx-2*math.pi*100*.0005))) for q in range(nx*16))/(nx*16)
        result.append(dict(nx=nx, substeps=steps, duration_s=.0005, density_l1_kg_m3=error,
            pressure_max_error_pa=max(abs(v-101325) for v in hotgas.primitive(c, final['conserved'])[1]),
            conservation_residual=receipt['conservation_residual']))
    return result


class HotGas(unittest.TestCase):
    def test_acoustic_transport_and_axis_symmetry(self):
        errors = []
        rho0 = 101325/(287*300); sound = math.sqrt(1.4*287*300); amplitude = 1e-5
        for nx in (32, 64):
            axis_errors = []
            for axis in range(3):
                c = config(); c['grid'][axis] = nx
                count = math.prod(c['grid']); stride = math.prod(c['grid'][:axis])
                sinc = math.sin(math.pi/nx)/(math.pi/nx); u = []
                for q in range(count):
                    wave = amplitude*sinc*math.sin(2*math.pi*((q//stride % nx)+.5)/nx)
                    rho = rho0*(1+wave); velocity = sound*wave
                    values = cell(rho, pressure=101325*(1+1.4*wave))
                    values[axis+1] = rho*velocity; values[4] += .5*rho*velocity**2
                    u += values
                source = [0.]*len(u); rate = (1, 20000*nx)
                initial = hotgas.initial(c, WORKER, u, source, rate)
                final, _ = hotgas.advance(c, WORKER, initial, source, 2*nx, rate)
                error = math.fsum(abs(final['conserved'][5*q] - rho0*(1+amplitude*sinc*
                    math.sin(2*math.pi*((q//stride % nx)+.5)/nx-2*math.pi*sound*.0001)))
                    for q in range(count))/count
                axis_errors.append(error)
            self.assertLess(max(axis_errors)-min(axis_errors), 1e-14)
            errors.append(axis_errors[0])
        self.assertGreater(errors[0]/errors[1], 1.8)
        self.assertLess(errors[1], rho0*amplitude*.01)

    def test_hot_cold_isobaric_eos(self):
        c = config(); u = (cell(101325/(287*300))+cell(101325/(287*1290)))*32
        ts, ps = hotgas.primitive(c, u)
        self.assertAlmostEqual(min(ts), 300); self.assertAlmostEqual(max(ts), 1290)
        self.assertAlmostEqual(u[5]/u[0], 300/1290)
        self.assertTrue(all(abs(v-101325) < 1e-9 for v in ps))
        s = hotgas.initial(c, WORKER, u, [0.]*len(u), (1, 100000))
        f, r = hotgas.advance(c, WORKER, s, [0.]*len(u), 10, (1, 100000))
        self.assertTrue(all(abs(v-101325) < 1e-8 for v in hotgas.primitive(c, f['conserved'])[1]))
        self.assertLess(max(abs(v) for v in r['conservation_residual']), 1e-8)

    def test_uniform_closed_volume_heating(self):
        c = config(); rho = 101325/(287*300); u = cell(rho)*64
        source = [0.]*len(u)
        cv = 287/.4
        for q in range(64): source[5*q+4] = rho*cv*(1290-300)/100
        s = hotgas.initial(c, WORKER, u, source, (1, 100000))
        f, r = hotgas.advance(c, WORKER, s, source, 100, (1, 100000))
        ts, ps = hotgas.primitive(c, f['conserved'])
        self.assertLess(max(abs(t-1290) for t in ts), 1e-8)
        self.assertLess(max(abs(p-101325*1290/300) for p in ps), 1e-6)
        self.assertEqual(f['conserved'][::5], u[::5])
        self.assertLess(max(abs(v) for v in r['conservation_residual']), 1e-7)

    def test_contact_spatial_convergence(self):
        rows = convergence(); errors = [r['density_l1_kg_m3'] for r in rows]
        self.assertTrue(errors[2] < errors[1] < errors[0])
        self.assertGreater(errors[1]/errors[2], 1.8)
        self.assertLess(max(r['pressure_max_error_pa'] for r in rows), 1e-7)

    def test_mass_momentum_energy_source(self):
        c = config(); u = cell()*64; source = ([1e-5, 1e-6, -2e-6, 3e-6, 1.])*64
        s = hotgas.initial(c, WORKER, u, source, (1, 100000))
        _, r = hotgas.advance(c, WORKER, s, source, 20, (1, 100000))
        for v in r['conservation_residual']: self.assertLess(abs(v), 1e-7)

    def test_exact_restart_and_identity(self):
        c, u = contact(16); source = [0.]*len(u); rate = (1, 100000)
        s = hotgas.initial(c, WORKER, u, source, rate)
        whole, _ = hotgas.advance(c, WORKER, s, source, 20, rate)
        middle, _ = hotgas.advance(c, WORKER, s, source, 7, rate)
        split, _ = hotgas.advance(c, WORKER, json.loads(json.dumps(middle)), source, 20, rate)
        self.assertEqual(whole, split)
        altered = copy.deepcopy(c); altered['gamma'] = 1.3
        changed = list(source); changed[4] = 1
        for cfg, src, clock in ((altered, source, rate), (c, changed, rate), (c, source, (1, 200000))):
            with self.assertRaises(ValueError): hotgas.advance(cfg, WORKER, middle, src, 20, clock)
        bad = copy.deepcopy(middle); bad['identity']['worker_sha256'] = '0'*64; bad = hotgas.sealed(bad)
        with self.assertRaises(ValueError): hotgas.advance(c, WORKER, bad, source, 20, rate)

    def test_refusal_immutable_checkpoint(self):
        c = config(); u = cell()*64; source = [0.]*len(u)
        s = hotgas.initial(c, WORKER, u, source, (1, 1)); before = copy.deepcopy(s)
        with self.assertRaises(ValueError): hotgas.advance(c, WORKER, s, source, 1, (1, 1))
        self.assertEqual(s, before)
        for value in (float('nan'), True):
            bad = list(u); bad[0] = value
            with self.assertRaises(ValueError): hotgas.initial(c, WORKER, bad, source, (1, 100000))
        with self.assertRaises(ValueError): hotgas.initial(c, WORKER, u, source, (True, 100000))


if __name__ == '__main__': unittest.main()
