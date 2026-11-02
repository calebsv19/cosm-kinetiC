"""Reject impossible cumulative work before starting an expensive trajectory."""
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.scalar_work_budget import require_horizon_work


class ScalarWorkBudgetTests(unittest.TestCase):
    def config(self, limit=1000000000):
        return dict(capacity_profile='scalar-wake128/v1', grid=[128]*3,
                    scalar_transport='mc_ssprk2/v1',
                    resource_limits=dict(scalar_work_cells=limit))

    def test_failed_campaign_was_impossible_at_minimum_cost(self):
        with self.assertRaisesRegex(ValueError, 'minimum_remaining=2684354560'):
            require_horizon_work(self.config(), 640)

    def test_restored_counter_is_cumulative(self):
        with self.assertRaisesRegex(ValueError, 'used=998244352'):
            require_horizon_work(self.config(), 174, first_step=172, used=998244352)

    def test_necessary_bound_is_not_adaptive_cost_guarantee(self):
        row = require_horizon_work(self.config(2684354560), 640)
        self.assertEqual(row['minimum_total'], 2684354560)
        self.assertTrue(row['adaptive_cost_unproven'])
        self.assertEqual(require_horizon_work(self.config(), 0)['minimum_total'], 0)

    def test_reject_before_source_mapping_or_root_creation(self):
        from coupling.bounded_replay import BoundedReplayRun
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)/'never-created'
            with self.assertRaisesRegex(ValueError, 'scalar work horizon cannot fit'):
                BoundedReplayRun(root, {'fluid': self.config()}, None,
                                 end_step=640, selected_steps=[])
            self.assertFalse(root.exists())

    def test_invalid_counter_and_horizon(self):
        for args in (dict(first_step=-1), dict(first_step=641),
                     dict(used=-1), dict(used=1000000001)):
            with self.subTest(args=args), self.assertRaises(ValueError):
                require_horizon_work(self.config(), 640, **args)


if __name__ == '__main__':
    unittest.main()
