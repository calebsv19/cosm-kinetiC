"""Four-cycle lifetime work is explicit and unavailable to legacy profiles."""
import copy
import json
import os
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import scalar_wake128_work as work
from coupling import scalar_wake128 as legacy
from coupling import scalar_gust128 as short
from coupling.scalar_work_budget import require_horizon_work


class Wake128WorkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.policy=json.loads(Path(os.environ['GUST128_POLICY']).read_text())

    def request(self):
        r=copy.deepcopy(self.policy);r.update(capacity_profile=work.PROFILE,state=None,steps=[])
        r['resource_limits']['scalar_work_cells']=work.WORK_LIMIT
        return r

    def test_full_horizon_and_adaptive_headroom(self):
        r=self.request();work.validate(r)
        self.assertEqual(work.WORK_LIMIT,10737418240)
        self.assertEqual(work.WORK_LIMIT,640*128**3*2*4)
        self.assertEqual(require_horizon_work(r,640)['minimum_total']*4,work.WORK_LIMIT)
        self.assertGreater(work.WORK_LIMIT,998244352+468*128**3*2*4)

    def test_exact_work_identity_and_legacy_profiles(self):
        r=self.request()
        for cap in (1000000000,work.WORK_LIMIT-1,work.WORK_LIMIT+1):
            q=copy.deepcopy(r);q['resource_limits']['scalar_work_cells']=cap
            with self.subTest(cap=cap),self.assertRaises(ValueError):work.validate(q)
        with self.assertRaises(ValueError):legacy.validate(r)
        with self.assertRaises(ValueError):short.validate(r)
        q=copy.deepcopy(r);q['capacity_profile']=legacy.PROFILE
        with self.assertRaises(ValueError):work.validate(q)

    def test_horizon_packet_and_fixed_physics_remain_closed(self):
        r=self.request()
        work.validate(dict(r,state={'data':{'steps':638}},steps=[{},{}]))
        for update in (dict(steps=[{}]*3),dict(state={'data':{'steps':639}},steps=[{},{}]),
                       dict(momentum_dt_s=.0025),dict(grid=[80]*3),
                       dict(scalar_transport='other')):
            with self.subTest(update=update),self.assertRaises(ValueError):work.validate(dict(r,**update))
        q=copy.deepcopy(r);q['open_domain_forcing']['knots'][1]['step']=81
        with self.assertRaises(ValueError):work.validate(q)

    def test_long_replay_still_requires_bounded_store(self):
        from coupling.transport_replay import ReplayRun
        with self.assertRaisesRegex(ValueError,'bounded retention'):
            ReplayRun('/unused',{'fluid':self.request(),'fluid_worker':'/unused'},None)


if __name__=='__main__':unittest.main()
