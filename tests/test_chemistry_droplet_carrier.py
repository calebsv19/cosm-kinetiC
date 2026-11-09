"""Qualification for coupled moist-gas transport and weighted parcels."""
import copy
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_droplet_carrier import AIR_CV, AIR_R, DropletCarrier, example
from chemistry_humid_droplet import R
from chemistry_stefan_interface import CV
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class DropletCarrierTests(unittest.TestCase):
    def test_solved_periodic_acoustic_refinement(self):
        errors = []
        for cells in (16,32,64):
            c = example(cells)
            c["dt_s"] = 1e-7
            c['droplets'] = []
            c['gas_velocity_m_s'] = [0.]*cells
            t0,p0 = 298.15,101325.
            seed = DropletCarrier(LIBRARY,c)
            rho,_,_,_,y,sound = seed.gas_fields(seed.state)[0]
            gas_constant = (1-y)*AIR_R+y*R
            gamma = 1+gas_constant/((1-y)*AIR_CV+y*CV)
            wave = [math.cos(math.tau*(i+.5)/cells) for i in range(cells)]
            p = [p0+2*w for w in wave]
            t = [t0*(q/p0)**(1-1/gamma) for q in p]
            partial = y*R/gas_constant
            rh = [partial*q/seed.exchange.properties(temp).pressure_pa for q,temp in zip(p,t)]
            steps = 2*cells
            c.update(gas_pressure_pa=p,gas_temperature_k=t,gas_relative_humidity=rh,
                     dt_s=c['length_m']/(2*math.sqrt(gamma*gas_constant*t0)*steps))
            m = DropletCarrier(LIBRARY,c)
            m.advance(steps)
            result = m.report()['droplet_carrier']
            errors.append(max(abs(q-(p0-2*w)) for q,w in zip(result['gas_pressure_pa'],wave))/2)
        print('periodic acoustic normalized errors',errors)
        self.assertGreater(errors[0]/errors[1],1.6)
        self.assertGreater(errors[1]/errors[2],1.7)

    def test_local_exchange_and_closed_budgets(self):
        c = example()
        droplet = DropletCarrier(LIBRARY,c)
        carrier = DropletCarrier(LIBRARY,dict(c,droplets=[]))
        initial = droplet.report()['droplet_carrier']
        droplet.advance(1000)
        carrier.advance(1000)
        r = droplet.report()['droplet_carrier']
        control = carrier.report()['droplet_carrier']
        p = r['parcels'][0]
        self.assertGreater(p['evaporated_kg'],1e-14)
        self.assertGreater(p['velocity_m_s'],.001)
        self.assertLess(p['temperature_k'],297.)
        vapor_gain = math.fsum(a-b for a,b in zip(r['gas_vapor_kg'],initial['gas_vapor_kg']))
        self.assertAlmostEqual(vapor_gain,p['evaporated_kg'],delta=1e-23)
        self.assertGreater(max(abs(a-b) for a,b in zip(r['gas_velocity_m_s'],control['gas_velocity_m_s'])),1e-8)
        self.assertGreater(max(abs(a-b) for a,b in zip(r['gas_pressure_pa'],control['gas_pressure_pa'])),1e-4)
        self.assertLess(abs(r['water_residual_kg']),1e-21)
        self.assertLess(abs(r['energy_residual_j']),1e-16)
        self.assertLess(abs(r['momentum_residual_kg_m_s']),1e-23)
        self.assertLess(abs(r['dry_air_residual_kg']),1e-21)

    def test_population_count_and_periodic_deposition(self):
        c = example()
        particle = dict(c['droplets'][0],count=3,position_m=0.)
        weighted = DropletCarrier(LIBRARY,dict(c,droplets=[particle]))
        split = DropletCarrier(LIBRARY,dict(c,droplets=[dict(particle,count=1) for _ in range(3)]))
        without = DropletCarrier(LIBRARY,dict(c,droplets=[]))
        a = weighted.derivative(weighted.state)
        b = without.derivative(without.state)
        deposited = [a[4*i+1]-b[4*i+1] for i in range(weighted.cells)]
        self.assertGreater(deposited[0],0.)
        self.assertAlmostEqual(deposited[0],deposited[-1],delta=1e-18)
        self.assertEqual(sum(x!=0. for x in deposited),2)
        self.assertAlmostEqual(math.fsum(deposited),-3*a[weighted.gas_end],delta=1e-18)
        weighted.advance(100)
        split.advance(100)
        for a,b in zip(weighted.state[:weighted.gas_end],split.state[:split.gas_end]):
            self.assertAlmostEqual(a,b,delta=1e-17)

    def test_population_bidirectional_and_restart(self):
        c = example()
        c['droplets'][0]['count'] = 20
        c['droplets'].append(dict(position_m=.016,velocity_m_s=.007,radius_m=15e-6,temperature_k=287.,count=8))
        m = DropletCarrier(LIBRARY,c)
        m.advance(500)
        copy_model = restore(LIBRARY,packet(m))
        m.advance(500)
        copy_model.advance(500)
        self.assertEqual(packet(m),packet(copy_model))
        r = m.report()['droplet_carrier']
        self.assertGreater(r['parcels'][0]['evaporated_kg'],1e-14)
        self.assertGreater(r['parcels'][1]['condensed_kg'],4e-15)
        self.assertLess(abs(r['water_residual_kg']),1e-21)
        with self.assertRaisesRegex(ValueError,'acoustic timestep'):
            DropletCarrier(LIBRARY,dict(copy.deepcopy(c),dt_s=1e-5))


if __name__=='__main__':
    unittest.main()
