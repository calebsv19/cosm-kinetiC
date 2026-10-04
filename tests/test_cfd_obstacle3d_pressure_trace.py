#!/usr/bin/env python3
"""Native pressure trace tested against independent analytic face quadrature."""
import json
import subprocess
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def pressure(x,y,z):
    return (.03+.004*x+.003*x*x+.002*y+.001*y*y-.001*z+.0007*z*z
            +.0011*x*y+.0009*x*z+.0005*y*z)


def analytic_force(center):
    # Two-point Gauss on each actual unit-square body face is exact for this
    # quadratic. This evaluates surface pressure, independently of cell averages.
    offset=.5/(3**.5);force=[0.,0.,0.]
    for axis in range(3):
        other=[a for a in range(3) if a!=axis]
        for side in (0,1):
            mean=0
            for a in (-offset,offset):
                for b in (-offset,offset):
                    p=[center,1,1];p[axis]+=(side-.5);p[other[0]]+=a;p[other[1]]+=b
                    mean+=pressure(*p)/4
            force[axis]+=(1 if side==0 else -1)*mean
    return force


class PressureTrace(unittest.TestCase):
    def test_limited_regular_pressure_control(self):
        rows=[]
        for length,counts in ((4,(8,16,32,48)),(8,(8,16,32,40))):
            errors=[]
            for n in counts:
                child=subprocess.run([str(ROOT/'build/c3d-obstacle/refinement-v2/pressure_trace_probe'),str(n),str(length),'cusp'],
                    cwd=ROOT,capture_output=True,text=True,check=True)
                row=json.loads(child.stdout);h=2/n
                means=[(2/3)*h**.5*((k+1)**1.5-k**1.5) for k in range(3)]
                bias=-.004*sum(w*m for w,m in zip((11/6,-7/6,2/6),means))
                self.assertLess(abs(row['force_n'][0]-(.015+bias)),1e-12)
                self.assertLess(row['gauge_error_n'],1e-11)
                row['independent_bias_n']=bias;errors.append(abs(bias));rows.append(row)
            self.assertTrue(all(a>b for a,b in zip(errors,errors[1:])))
            self.assertLess(abs(errors[-1]/errors[0]-(counts[0]/counts[-1])**.5),1e-12)
        (ROOT/'build/c3d-obstacle/refinement-v2/native-pressure-cusp-control.json').write_text(json.dumps({'passed':True,'rows':rows,
          'scope':'known square-root pressure trace counterexample to smooth-polynomial accuracy; not a Stokes solution or measured cube singularity exponent'},indent=2)+'\n')

    def test_both_lengths_refinement_and_gauge(self):
        rows=[]
        for length,counts in ((4,(8,16,32,48)),(8,(8,16,32,40))):
            exact=analytic_force(length/2)
            for n in counts:
                child=subprocess.run([str(ROOT/'build/c3d-obstacle/refinement-v2/pressure_trace_probe'),str(n),str(length)],
                                     cwd=ROOT,capture_output=True,text=True,check=True)
                row=json.loads(child.stdout)
                self.assertLess(max(abs(a-b) for a,b in zip(row['force_n'],exact)),1e-11)
                self.assertLess(row['gauge_error_n'],1e-11)
                row['independent_surface_force_n']=exact;rows.append(row)
        (ROOT/'build/c3d-obstacle/refinement-v2/native-pressure-known-answer.json').write_text(json.dumps({'passed':True,'rows':rows},indent=2)+'\n')

if __name__=='__main__':unittest.main()
