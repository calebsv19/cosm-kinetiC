"""Independent comparison geometry and immutable native input checks."""
import array,hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.local_transport_report import pair,read,region_difference


class LocalTransportTests(unittest.TestCase):
    def test_constant_extensive_fields_restrict_without_error(self):
        def field(n):return dict(energy_j=[1/n**3]*n**3,smoke_kg=[.001/n**3]*n**3)
        p=pair(field(8),field(16),8,16)
        self.assertEqual(p['source_region_heat_relative_l1'],0.)
        self.assertLess(p['full_fields']['energy_j']['relative_l1'],1e-14)
        self.assertEqual(p['diagnostic_flux_max_absolute_difference_w_m2'],0.)

    def test_local_metric_excludes_changes_outside_declared_region(self):
        a=[1.]*512;b=a.copy();b[-1]=1000.
        self.assertEqual(region_difference(a,b,8),0.)
        q=(1*8+3)*8+3;b[q]=2.
        self.assertAlmostEqual(region_difference(a,b,8),1/49)

    def test_native_read_refuses_changed_fields_and_wrong_case(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);case=root/'case';case.mkdir();n=8
            values=array.array('d',[.001]*n**3+[0.]*n**3+[0.]*(3*n**3+3*n*n))
            path=case/'fields.bin';path.write_bytes(values.tobytes())
            summary=dict(grid=n,dt_s=.005,time_s=3.2,box=False,fields_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),heat_balance_j=0.,smoke_balance_kg=0.,divergence=0.,pressure_residual=0.,peak_temperature_k=310.)
            (case/'summary.json').write_text(json.dumps(summary))
            self.assertEqual(len(read(root,'case',n,.005,False)[1]['energy_j']),512)
            with self.assertRaises(ValueError):read(root,'case',n,.005,True)
            values[0]=.002;path.write_bytes(values.tobytes())
            with self.assertRaises(ValueError):read(root,'case',n,.005,False)

if __name__=='__main__':unittest.main()
