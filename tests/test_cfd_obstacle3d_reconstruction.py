"""Independent analytic curl/strain and Gauss quadrature checks; development reference venv."""
import json,subprocess,tempfile,unittest,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from probe_cfd_obstacle3d_strain import reconstruct,energy,trace
class Reconstruction(unittest.TestCase):
 def test_exact_continuous_quadrature_and_known_answer_refinement(self):
  rows=[]
  with tempfile.TemporaryDirectory() as root:
   for n in (8,16,32,48):
    artifact=Path(root)/f'{n}.bin'
    p=subprocess.run([str(ROOT/'build/c3d-obstacle/correction-v1/reconstruction-probe-corrected'),str(n),'average',str(artifact)],check=True,capture_output=True,text=True)
    r=json.loads(p.stdout);nodes,h,lo,hi=reconstruct(artifact)
    d,div=energy(nodes,h,lo,hi);f=trace(nodes,h,lo,hi)
    self.assertLess(abs(d/r['dissipation_w']-1),1e-10)
    self.assertLess(abs(f[0]/r['viscous_force_n']-1),1e-8)
    self.assertLess(r['divergence'],1e-9)
    r['independent_dissipation_w']=d;r['reconstruction_divergence_l2']=div;rows.append(r)
  errors=[abs(r['dissipation_w']/r['exact_dissipation_w']-1) for r in rows]
  self.assertTrue(all(a>b for a,b in zip(errors,errors[1:])),errors)
  self.assertLess(errors[-1],.065) # High-degree curl is intentionally underresolved on coarse meshes.
  p_errors=[abs(r['pressure_force_n']/r['exact_pressure_force_n']-1) for r in rows]
  self.assertTrue(all(a>b for a,b in zip(p_errors,p_errors[1:])))
  self.assertLess(abs(rows[-1]['viscous_force_n']/rows[-1]['exact_viscous_force_n']-1),.06)
  (ROOT/'build/c3d-obstacle/correction-v1/reconstruction-qualification.json').write_text(json.dumps({'rows':rows,'energy_errors':errors,'pressure_errors':p_errors,'continuous_quadrature_passed':True,'scope':'known analytic curl; high-degree coarse controls retained; no universal force certificate'},indent=2)+'\n')
if __name__=='__main__':unittest.main()
