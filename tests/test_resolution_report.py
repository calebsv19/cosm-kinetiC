"""Independent capacity, common-partition trend and stabilization checks."""
import array,hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_accuracy import dimensions,parse_grid
from coupling.directional_report import read
from coupling.resolution_report import refinement_pattern,stabilization

class ResolutionTests(unittest.TestCase):
    def test_capacity_requires_explicit_exact_profile(self):
        with self.assertRaises(ValueError):dimensions(128)
        self.assertEqual(parse_grid('128',full_grid=True),128)
        for grid in ([64,64,128],256,8,True):
            with self.assertRaises(ValueError):dimensions(grid,full_grid=True)

    def test_uniform_first_order_error_has_half_difference_on_same_partition(self):
        grids=[[n]*3 for n in (8,16,32)]
        fields=[dict(energy_j=[(10+1/n)/n**3]*n**3) for n in (8,16,32)]
        result=refinement_pattern(fields,grids,[8]*3)
        self.assertAlmostEqual(result['difference_ratio'],.5)
        self.assertAlmostEqual(result['observed_order_indicator'],1.)
        self.assertAlmostEqual(result['difference_pattern_cosine'],1.)
        self.assertAlmostEqual(result['absolute_difference_j'][0],.09375/16)

    def test_screen_preserves_ten_percent_boundary_and_rejects_undefined_tail(self):
        p=dict(source_region_heat_relative_l1=.1,height_profile_relative_l1=.05,
               full_fields={'smoke_kg':{'relative_l1':.08}},wind_probes_m_s=[{}, {'probe':[.1,0,0]}],wind_probe_difference_l2_m_s=.005)
        s=stabilization(p,{'peak_temperature_k':310.5},{'peak_temperature_k':310.})
        self.assertTrue(s['all_passed']);self.assertAlmostEqual(s['relative_changes']['aggregate_wind_probe_l2'],.05)
        p['source_region_heat_relative_l1']=.100001
        self.assertFalse(stabilization(p,{'peak_temperature_k':310.5},{'peak_temperature_k':310.})['all_passed'])
        p['height_profile_relative_l1']=None
        self.assertFalse(stabilization(p,{'peak_temperature_k':310.5},{'peak_temperature_k':310.})['passed']['height_temperature_rise_profile_l1'])

    def test_native_full_profile_identity_and_resources_are_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);n=32;count=n**3
            data=array.array('d',[1/count]*count+[.001/count]*count+[0.]*(3*count+3*n*n)).tobytes()
            (p/'fields.bin').write_bytes(data)
            s=dict(grid=n,dt_s=.005,time_s=3.2,transport_scheme='muscl_minmod',box=False,
                   fields_sha256=hashlib.sha256(data).hexdigest(),heat_balance_j=0.,smoke_balance_kg=0.,
                   divergence=0.,pressure_residual=0.,peak_temperature_k=310.)
            (p/'summary.json').write_text(json.dumps(s))
            with self.assertRaisesRegex(ValueError,'profile/resources'):read(p,full_grid=True)
            s.update(qualification_profile='full_grid_128_v1',scalar_transport_scheme='muscl_minmod',
                     memory_limit_bytes=2147483648,memory_peak_bytes=1000000,scalar_work_cells=100,
                     max_scalar_work_cells=8000000000)
            (p/'summary.json').write_text(json.dumps(s));self.assertEqual(read(p,full_grid=True)[2],[32]*3)
            s['memory_peak_bytes']=2147483649;(p/'summary.json').write_text(json.dumps(s))
            with self.assertRaisesRegex(ValueError,'profile/resources'):read(p,full_grid=True)

if __name__=='__main__':unittest.main()
