"""Independent face integrals and limited-regularity controls for candidate trace."""
import unittest,sys,json,hashlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_four_interval_projection import interval_trace

def mean_power(a,b,k):return (b**(k+1)-a**(k+1))/((k+1)*(b-a))
class FourInterval(unittest.TestCase):
    def test_all_cubic_monomials_against_independent_face_integrals(self):
        lo=np.array([1.5,.5,.5]);hi=np.array([2.5,1.5,1.5])
        for powers in [(i,j,k) for i in range(4) for j in range(4-i) for k in range(4-i-j)]:
            for axis in range(3):
                tangential=np.prod([mean_power(lo[b],hi[b],powers[b]) for b in range(3) if b!=axis])
                exact=tangential*(lo[axis]**powers[axis]-hi[axis]**powers[axis])
                for h in (.125,.0625,.03125):
                    traces=[]
                    for side in (0,1):
                        row=[]
                        for depth in range(4):
                            a=lo[axis]-(depth+1)*h if side==0 else hi[axis]+depth*h
                            b=lo[axis]-depth*h if side==0 else hi[axis]+(depth+1)*h
                            row.append(tangential*mean_power(a,b,powers[axis]))
                        traces.append(interval_trace(row))
                        self.assertAlmostEqual(interval_trace(np.asarray(row)+3.2)-traces[-1],3.2,places=11)
                    self.assertAlmostEqual(traces[0]-traces[1],exact,places=11)
    def test_square_root_trace_retains_half_order_bias(self):
        biases=[]
        for h in (.125,.0625,.03125):
            averages=[(2/3)*h**.5*((d+1)**1.5-d**1.5) for d in range(4)]
            biases.append(abs(interval_trace(averages)))
        self.assertGreater(biases[0],0)
        self.assertAlmostEqual(biases[1]/biases[0],2**(-.5),places=12)
        self.assertAlmostEqual(biases[2]/biases[1],2**(-.5),places=12)
    def test_only_declared_projection_and_routing_transform(self):
        for t in json.loads((R/'build/c3d-pressure-four-interval/transforms.json').read_text()):
            parent=(R/t['parent']).read_bytes();output=(R/t['output']).read_bytes()
            self.assertEqual(hashlib.sha256(parent).hexdigest(),t['parent_sha256'])
            self.assertEqual(hashlib.sha256(output).hexdigest(),t['output_sha256'])
            s=parent.decode()
            for a,b in t['literal_replacements']:self.assertIn(a,s);s=s.replace(a,b)
            self.assertEqual(s,output.decode())
if __name__=='__main__':unittest.main()
