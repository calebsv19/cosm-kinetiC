import os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.validation_campaign import configuration_for
from coupling.driven_wind import integrated_inlet


class Campaign(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workers=tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT',
            'FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_OPEN_WORKER','FIRE_FLUID_CLOCK_WORKER'))

    def test_cadence_changes_only_exchange_and_packetization(self):
        a=configuration_for(self.workers,'cadence-100ms')
        b=configuration_for(self.workers,'cadence-40ms')
        for key in ('source','fluid','receiver'):self.assertEqual(a[key],b[key])
        self.assertEqual(a['fluid']['grid'],[64]*3)
        self.assertEqual(a['periods'][:2],b['periods'][:2])
        self.assertEqual((a['windows'],b['windows']),(12,30))
        self.assertEqual(a['fluid']['wind_forcing']['knots'],
            [{'step':0,'speed_m_s':0},{'step':80,'speed_m_s':.5},{'step':240,'speed_m_s':0}])

    def test_ventilation_equal_physical_inlet_volume(self):
        cases=[configuration_for(self.workers,n) for n in ('vent-full','vent-elevated','vent-obstacle')]
        for c in cases:self.assertAlmostEqual(integrated_inlet(c['fluid'],480),.525,places=12)
        for c in cases[1:]:self.assertEqual(c['source'],cases[0]['source'])
        self.assertNotIn('solid_box',cases[0]['fluid'])
        self.assertIn('solid_box',cases[-1]['fluid'])

    def test_unknown_case(self):
        with self.assertRaises(ValueError):configuration_for(self.workers,'unknown')


if __name__=='__main__':unittest.main()
