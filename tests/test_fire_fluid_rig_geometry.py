"""Independent geometry/port/connectivity fixtures, without running a solver."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import rig_geometry as rig


def config(n,opening):
    return dict(grid=[n]*3,length_m=[1]*3,rig_geometry=dict(schema=rig.SCHEMA,window_open=opening),
                boundary_policy=dict(horizontal='ambient_open_xy',vertical='solid_bottom_open_top'),open_domain_forcing={})


class GeometryTests(unittest.TestCase):
    def test_render_box_union_matches_mask_at_both_resolutions(self):
        for n in (16,32):
            for opening in (False,True):
                c=config(n,opening);mask=rig.topology(c);boxes=rig.solid_boxes(c)
                expected=[]
                for k in range(n):
                    for j in range(n):
                        for i in range(n):
                            p=[(i+.5)/n,(j+.5)/n,(k+.5)/n]
                            expected.append(int(any(all(lo[a]<=p[a]<hi[a] for a in range(3)) for _,lo,hi in boxes)))
                self.assertEqual(mask,expected)
                components=rig.components(c,mask);self.assertEqual(len(components),1 if opening else 2)
                self.assertTrue(all(x['pressure_reservoir'] for x in components))
    def test_partition_and_ports_have_exact_physical_apertures(self):
        for n in (16,32):
            for opening in (False,True):
                c=config(n,opening);mask=rig.topology(c)
                partition=sum(not rig.blocked(c,mask,0,n//2,j,k) for j in range(n) for k in range(n))
                self.assertEqual(partition/n**2,.125 if opening else 0)
                west=sum(not rig.blocked(c,mask,0,0,j,k) for j in range(n) for k in range(n));east=sum(not rig.blocked(c,mask,0,n,j,k) for j in range(n) for k in range(n))
                self.assertEqual(west/n**2,.25);self.assertEqual(east,west)
    def test_refuses_unadmitted_geometry_modes(self):
        for c in [config(24,True),dict(config(16,True),rig_geometry=dict(schema=rig.SCHEMA,window_open=1))]:
            with self.assertRaises(ValueError):rig.topology(c)

if __name__=='__main__':unittest.main()
