"""Analytic rectangular sampling, restriction and probe bounds."""
import array,sys,tempfile,unittest,hashlib,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_accuracy import dimensions,parse_grid,fields
from coupling.convergence import comparison
from coupling.directional_report import region_difference,pair
from coupling.surface_observations import mean_temperature,observe
class DirectionalTests(unittest.TestCase):
    def test_rectangular_constant_temperature_and_extensive_restriction(self):
        a=[1206*7/(8*16*32)]*(8*16*32)
        b=[1206*7/(16*16*32)]*(16*16*32)
        self.assertAlmostEqual(mean_temperature(a,[8,16,32],[.13,.2,.1],[.68,.7,.9]),307.)
        self.assertAlmostEqual(observe(a,[8,16,32])['faces']['x+']['gas_temperature_k'],307.)
        self.assertEqual(comparison(dict(energy_j=a,smoke_kg=a),dict(energy_j=b,smoke_kg=b),[8,16,32],[16,16,32])['energy_j']['relative_l1'],0.)
    def test_region_is_same_physical_volume_on_rectangular_grid(self):
        grid=[8,16,32];a=[1.]*(8*16*32);b=a.copy();b[-1]=1000
        self.assertEqual(region_difference(a,b,grid),0.)
        b[(1*16+5)*8+3]=2
        self.assertAlmostEqual(region_difference(a,b,grid),1/(12*8*4+1))
    def test_bounds_and_field_length(self):
        self.assertEqual(parse_grid('64x64x128'),[64,64,128])
        for grid in ([128]*3,[7,8,8],[8,8], [True,8,8],True,None):
            with self.assertRaises(ValueError):dimensions(grid)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'fields.bin';p.write_bytes(array.array('d',[0.]*(5*8*16*32+8*16+16*32+8*32)).tobytes())
            self.assertEqual(len(fields(p,[8,16,32])['energy_j']),8*16*32)
            p.write_bytes(p.read_bytes()[:-8])
            with self.assertRaises(ValueError):fields(p,[8,16,32])
    def test_comparison_rejects_source_identity_and_field_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name,grid in [('a',[8,8,8]),('b',[16,8,8])]:
                d=root/name;d.mkdir();count=grid[0]*grid[1]*grid[2]
                faces=3*count+grid[0]*grid[1]+grid[0]*grid[2]+grid[1]*grid[2]
                data=array.array('d',[1/count]*count+[.001/count]*count+[0.]*faces).tobytes()
                (d/'fields.bin').write_bytes(data)
                s=dict(grid=grid,dt_s=.005,time_s=3.2,box=False,fields_sha256=hashlib.sha256(data).hexdigest(),source_frames_sha256='a'*64,probe_sha256='b'*64,heat_balance_j=0.,smoke_balance_kg=0.,divergence=0.,pressure_residual=0.,peak_temperature_k=310.)
                (d/'summary.json').write_text(json.dumps(s))
            self.assertEqual(pair(root/'a',root/'b')['source_region_heat_relative_l1'],0.)
            s['source_frames_sha256']='c'*64;(root/'b/summary.json').write_text(json.dumps(s))
            with self.assertRaises(ValueError):pair(root/'a',root/'b')
            s['source_frames_sha256']='a'*64;(root/'b/summary.json').write_text(json.dumps(s))
            data=bytearray(data);data[7]^=1;(root/'b/fields.bin').write_bytes(data)
            with self.assertRaises(ValueError):pair(root/'a',root/'b')
if __name__=='__main__':unittest.main()
