"""Independent physical slab allocations and selected native tall-domain bounds."""
import copy
import math
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'tests'))
from test_open_atmosphere import request,WORKER
from test_surface_source_receiver import policy,FIXTURES
from surface_sources.growth_fire_v1 import strict_load
from surface_sources.receiver import allocate,mapping,policy_valid
from open_atmosphere import validate
from plume_qualification import write_schedule,run_sparse,read_sample

class DomainTests(unittest.TestCase):
    def test_fractional_slab_known_answer_and_solid_rejection(self):
        f=strict_load(FIXTURES/'prescribed-patch.json');p=policy(f)
        p['receiver'].update(injection_depth_m=.15,injection_fluid_mask=[1]*98)
        a=allocate(f,p,[0,.05],'slab')['steps'][0]['cells']
        self.assertEqual([x['cell_index'] for x in a],[0,1,7,8,49,50,56,57])
        for x in a:self.assertAlmostEqual(x['energy_transferred_j'],1000 if x['cell_index']<49 else 500,places=10)
        self.assertAlmostEqual(math.fsum(x['energy_transferred_j'] for x in a),6000,places=10)
        p['receiver']['injection_fluid_mask'][49]=0
        with self.assertRaises(ValueError):mapping(f,p)
        p['receiver']['injection_depth_m']=.21
        with self.assertRaises(ValueError):policy_valid(p)

    def test_fixed_physical_support_at_three_resolutions(self):
        f=strict_load(FIXTURES/'prescribed-patch.json')
        for n in (32,48,64):
            p=policy(f);dz=2/n;depth=.0625;layers=math.ceil(depth/dz)
            p['receiver'].update(dimensions=[n,n,n],spacing_m=[dz]*3,fluid_mask=[1]*(n*n),
                injection_depth_m=depth,injection_fluid_mask=[1]*(n*n*layers))
            for row in mapping(f,p):
                self.assertAlmostEqual(math.fsum(w for _,w in row),1,places=14)
                for z in range(layers):
                    expected=min(dz,depth-z*dz)/depth
                    self.assertAlmostEqual(math.fsum(w for q,w in row if q//(n*n)==z),expected,places=14)

    def test_selected_native_tall_packet_and_ordinary_rejection(self):
        r=request(128,.005);r['steps']=[]
        r['properties']['dynamic_viscosity_pa_s']=.00002
        r['resource_limits']={'max_cells':524288,'scalar_work_cells':1000000000,
            'cache_pressure_operator':True,'numerical_bytes':512*1024*1024}
        r['boundary_policy'].update(vertical='solid_bottom_open_top',predictor_velocity='no_slip_bottom_zero_gradient_top')
        with self.assertRaises(ValueError):validate(r)
        with self.assertRaises(ValueError):validate(r,movie=True)
        validate(r,domain_qualification=True)
        with self.assertRaises(ValueError):validate(r,movie=True,domain_qualification=True)
        wrong=copy.deepcopy(r);wrong['grid'][0]=128
        with self.assertRaises(ValueError):validate(wrong,domain_qualification=True)
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);s=root/'forcing.bin'
            rows=[[{'cell_index':0,'energy_transferred_j':.001,'smoke_transferred_kg':1e-8}]]*2
            totals=write_schedule(s,r,rows,[2],domain_qualification=True)
            receipt=run_sparse(r,WORKER,s,root/'native',timeout_s=30,domain_qualification=True)
            self.assertEqual(receipt['schema'],'physics_sim_sparse_domain_receipt/v1')
            result=read_sample(root/'native/sample-0002.bin',r,receipt['worker_sha256'],totals[-1],
                compact=True,domain_qualification=True)
            self.assertEqual(result['schema'],'physics_sim_domain_sample_fields/v1')
            self.assertEqual(result['fields']['time_s'],.01)
            self.assertAlmostEqual(result['budgets']['energy_j']['input'],.002)
            self.assertFalse(result['native_checkpoint_restart'])
            with self.assertRaises(ValueError):run_sparse(r,WORKER,s,root/'ordinary',timeout_s=30)

if __name__=='__main__':unittest.main()
