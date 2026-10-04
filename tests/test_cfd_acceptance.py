import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts/agent_session'))
from acceptance import assess

class AcceptanceTests(unittest.TestCase):
    def snapshot(self):
        return {'state':'completed','physics':{'density_kg_m3':1,'mean_inlet_speed_m_s':.002,'dimensions_m':[4,2,.5]},
                'health':{'volume_flux_m3_s':.002,'mass_balance_residual_kg_s':0,'max_divergence_s_inv':0,'projection_status':'converged'},
                'steady_acceptance':{'status':'passed'},'energy_budget':{'available':True,'relative_residual':.01},
                'boundary_force_budget':{'available':True,'surface_total_force_x_n':1,'control_volume_force_x_n':1}}
    def comparisons(self):
        changes={k:{'relative_to_fine':.001} for k in ['kinetic_energy_j','volume_flux_m3_s','surface_total_force_x_n','control_volume_force_x_n']}
        return {k:{'comparisons':[{'changes':copy.deepcopy(changes)}]} for k in ['spatial','temporal']}
    def test_missing_and_nonfinite_evidence(self):
        s=self.snapshot();self.assertEqual(assess(s)['status'],'not_established')
        for value in [None,float('nan'),float('inf')]:
            s['health']['mass_balance_residual_kg_s']=value
            self.assertEqual(assess(s,self.comparisons())['status'],'not_established')
    def test_independent_gates_and_no_certification(self):
        s=self.snapshot();c=self.comparisons();a=assess(s,c)
        self.assertEqual(a['status'],'passed');self.assertFalse(a['physical_accuracy_certified'])
        s['energy_budget']['relative_residual']=.021
        self.assertEqual(assess(s,c)['status'],'failed')
        s=self.snapshot();s['boundary_force_budget']['surface_total_force_x_n']=1.1
        self.assertEqual(assess(s,c)['status'],'failed')
        s=self.snapshot();s['state']='cancelled';self.assertEqual(assess(s,c)['status'],'failed')
    def test_missing_authored_body_force_cannot_pass(self):
        s=self.snapshot();s['physics']['stationary_obstacle']=True;s['boundary_force_budget']={}
        report=assess(s,self.comparisons())
        self.assertEqual(report['gates']['surface_control_volume_force']['status'],'not_established')
        self.assertEqual(report['status'],'not_established')

    def test_refinement_and_steady_are_required(self):
        s=self.snapshot();c=self.comparisons()
        c['spatial']['comparisons'][0]['changes']['surface_total_force_x_n']['relative_to_fine']=.04
        self.assertEqual(assess(s,c)['gates']['spatial_refinement']['status'],'failed')
        s['steady_acceptance']['status']='not_established'
        self.assertEqual(assess(s,self.comparisons())['status'],'not_established')
if __name__=='__main__':unittest.main(verbosity=2)
