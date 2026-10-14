import copy
import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import hotgas_domain as domain, lowmach_cases as cases
from coupling.hotgas_domain_cases import config,face,hydrostatic
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']


class ContactTests(unittest.TestCase):
    def test_exact_stationary_contact_and_retained_diffusion(self):
        contact=cases.stationary_contact(WORKER,'hllc');old=cases.stationary_contact(WORKER,'rusanov')
        self.assertLess(contact['max_density_error_kg_m3'],1e-12)
        self.assertGreater(old['max_density_error_kg_m3'],.1)

    def test_acoustic_phase_and_refinement(self):
        rows=[cases.acoustic(WORKER,'hllc',n) for n in (16,32,64)]
        errors=[r['relative_pressure_l1_error'] for r in rows]
        self.assertLess(errors[-1],.07)
        for a,b in zip(errors,errors[1:]): self.assertGreater(a/b,1.6)

    def test_contact_policy_restart_and_hydrostatic_equilibrium(self):
        c=config();c['domain']['flux']='hllc';u=hydrostatic(c);source=[0.]*len(u);rate=(1,1000000)
        initial=domain.initial(c,WORKER,u,source,rate)
        final,receipt=domain.advance(c,WORKER,initial,source,1000,rate)
        self.assertLess(max(abs(a-b) for a,b in zip(u,final['conserved'])),1e-7)
        middle,_=domain.advance(c,WORKER,initial,source,177,rate)
        restart,_=domain.advance(c,WORKER,json.loads(json.dumps(middle)),source,1000,rate)
        self.assertEqual(final,restart)
        altered=copy.deepcopy(c);altered['domain']['flux']='rusanov'
        with self.assertRaises(ValueError): domain.advance(altered,WORKER,middle,source,1000,rate)
        altered['domain']['flux']='unknown'
        with self.assertRaises(ValueError): domain.validate(altered)
        bad=domain.initial(c,WORKER,u,source,(1,1));before=copy.deepcopy(bad)
        with self.assertRaises(ValueError): domain.advance(c,WORKER,bad,source,1,(1,1))
        self.assertEqual(bad,before)

    def test_hllc_closed_wall_and_open_throughflow(self):
        # Existing known-ledger tests parameterized here; no duplicated implementation oracle.
        import test_hotgas_domain as old
        original=old.config
        def contact_config(*args,**kwargs):
            c=original(*args,**kwargs);c['domain']['flux']='hllc';return c
        old.config=contact_config
        try:
            for name in ('test_closed_slip_walls_zero_mass_and_energy_leak',
                         'test_uniform_reservoir_throughflow_known_face_budgets',
                         'test_multiaxis_hydrostatic_open_reservoir',
                         'test_hot_parcel_initial_buoyant_response_and_potential_energy',
                         'test_conductive_wall_transfer_is_conservative_and_signed'):
                old.DomainTests(name).debug()
        finally: old.config=original


if __name__=='__main__': unittest.main()
