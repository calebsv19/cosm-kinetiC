"""Reference acceptance cannot be inferred from completion or a cached boolean."""
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts/agent_session'))
from refined import assess_refined


def status():
    return {'state':'completed', 'solve_mode':'steady_stokes',
            'health':{'projection_status':'converged', 'linear_relative_residual':1e-12,
                      'max_abs_divergence_s_inv':1e-10},
            'energy_budget':{'steady_stokes_relative_residual':.005},
            'qualification':{'fixed_case_reference_gate':{'applicable':True,'passed':True,
                 'pressure_relative_error':.01, 'viscous_relative_error':.01,
                 'total_relative_error':.001}}}


class AssessmentTests(unittest.TestCase):
    def test_separate_force_components_cannot_cancel(self):
        row=status()
        row['qualification']['fixed_case_reference_gate']['pressure_relative_error']=.05
        result=assess_refined(row)
        self.assertEqual(result['reference_accuracy']['status'], 'failed')
        self.assertFalse(result['physical_accuracy_certified'])

    def test_requires_complete_finite_observations_and_convergence(self):
        base=status()
        self.assertEqual(assess_refined(base)['reference_accuracy']['status'], 'passed')
        paths=[('health','linear_relative_residual'),('health','max_abs_divergence_s_inv'),
               ('energy_budget','steady_stokes_relative_residual')]
        for group,key in paths:
            for value in (None, True, -1, float('nan'), float('inf')):
                row=copy.deepcopy(base);row[group][key]=value
                with self.subTest(group=group,key=key,value=value):
                    self.assertNotEqual(assess_refined(row)['reference_accuracy']['status'],'passed')
        row=status();row['state']='failed'
        self.assertNotEqual(assess_refined(row)['reference_accuracy']['status'],'passed')
        row=status();row['health']['linear_relative_residual']=1e-3
        self.assertNotEqual(assess_refined(row)['reference_accuracy']['status'],'passed')
        row=status();del row['qualification']['fixed_case_reference_gate']['viscous_relative_error']
        self.assertEqual(assess_refined(row)['reference_accuracy']['status'],'not_established')

    def test_inapplicable_reference_is_distinct(self):
        row=status();row['qualification']['fixed_case_reference_gate']['applicable']=False
        result=assess_refined(row)['reference_accuracy']
        self.assertEqual(result['status'],'not_applicable')
        self.assertEqual(result['checks'],{})


if __name__=='__main__':unittest.main()
