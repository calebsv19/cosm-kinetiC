import math,sys,unittest,tempfile,subprocess,json
from unittest.mock import patch
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from fire_fluid_sharp_transport import reference,A,B,U,run

class SharpTransportReference(unittest.TestCase):
    def test_pure_translation_support_and_amount(self):
        n=256;z=np.arange(n)/n
        v=reference(z,z+1/n,3.2,0)
        self.assertAlmostEqual(float(v.sum()/n),(B-A)*3.2,places=12)
        self.assertEqual(float(reference(14/32,15/32,3.2,0)),0)
        self.assertEqual(float(reference(11/32,12/32,1.5,0)),0)
        # At x=.2, after enough time all age [0,(x-A)/u] contributes.
        eps=1e-5
        self.assertAlmostEqual(float(reference(.2-eps,.2+eps,3.2,0)),(.2-A)/U,places=9)
    def test_diffusive_mass_and_quadrature(self):
        n=256;z=np.arange(n)/n
        for t in [.1,1.5,3.2]:
            v=reference(z,z+1/n,t,.026)
            self.assertAlmostEqual(float(v.sum()/n),(B-A)*t,places=11)
    def test_independent_point_quadrature(self):
        # Separate spatial quadrature of pointwise source-CDF convolution;
        # does not use the production reference's integrated-erf primitive.
        q,w=np.polynomial.legendre.leggauss(256);age=3.2*(q+1)/2;tw=3.2*w/2
        width=2*np.sqrt(.026/1206*age);erf=np.vectorize(math.erf)
        x,xw=np.polynomial.legendre.leggauss(64)
        for lo,hi in [(11/32,12/32),(14/32,15/32)]:
            z=(lo+hi)/2+(hi-lo)/2*x[:,None]
            values=.5*(erf((z-A-U*age)/width)-erf((z-B-U*age)/width))
            expected=float((xw/2)@values@tw)
            actual=float(reference(lo,hi,3.2,.026))
            self.assertLess(abs(actual/expected-1),1e-8)

    def test_timeout_preserves_partial_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);probe=root/'probe';probe.write_bytes(b'fixture executable identity')
            error=subprocess.TimeoutExpired(['probe'],45,output=b'partial output\n',stderr=b'partial diagnostic\n')
            with patch('fire_fluid_sharp_transport.subprocess.run',side_effect=error):
                with self.assertRaises(subprocess.TimeoutExpired):run(probe,root/'run')
            self.assertEqual((root/'run/case-0.jsonl').read_text(),'partial output\n')
            self.assertFalse(json.loads((root/'run/case-0-receipt.json').read_text())['complete'])

    def test_selected_case_is_single_bounded_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);probe=root/'probe';probe.write_bytes(b'fixture executable identity')
            error=subprocess.TimeoutExpired(['probe'],180,output=b'partial selected output')
            with patch('fire_fluid_sharp_transport.subprocess.run',side_effect=error) as launch:
                with self.assertRaises(subprocess.TimeoutExpired):
                    run(probe,root/'selected','finest-half-step')
            launch.assert_called_once_with([str(probe),'1024','0.5','5120','0.026','mc'],
                capture_output=True,text=True,timeout=180)
            identity=json.loads((root/'selected/identity.json').read_text())
            self.assertEqual(identity['threshold_relative'],.1)
            self.assertEqual(identity['cases'],[[1024,.5,5120,.026,'mc']])
            self.assertFalse((root/'selected/report.json').exists())
            with self.assertRaises(ValueError):run(probe,root/'invalid','unknown')
            self.assertFalse((root/'invalid').exists())

if __name__=='__main__':unittest.main()
