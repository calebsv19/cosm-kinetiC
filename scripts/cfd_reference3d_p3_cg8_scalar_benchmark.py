"""Once-only timed original small-cube coarse action; no numerical field."""
import json,time,resource,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_distributed_p3_condensed import DistributedP3CondensedSystem
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_domain_budget import enforce_phase
from cfd_reference3d_p3_cg8_scalar import RefinedFloatCoarse
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-p3-cg8-scalar'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'support/action-benchmark.json';assert not out.exists();start=time.monotonic()
 def sample(phase):enforce_phase(phase,resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
 mesh,*_=domain_mesh(4.,2,False,3,1,'original');s=DistributedP3CondensedSystem(mesh,.1,fixed_boundaries=('walls','body'),stage_callback=sample);C=s.p3.upper;f=RefinedFloatCoarse(C,D/'support/coarse.dylib',numeric=False);rng=np.random.default_rng(3852);x=rng.normal(size=f.n)
 def old(x):return C@x+C.T@x-C.diagonal()*x
 expected=old(x);actual=f.action(x);error=float(np.linalg.norm(actual-expected)/np.linalg.norm(expected));assert error<=1e-12;times={'scipy':[],'native':[]}
 for batch in range(3):
  for label,action in ((('scipy',old),('native',f.action)) if batch%2==0 else (('native',f.action),('scipy',old))):
   begin=time.monotonic()
   for i in range(100):y=action(x)
   times[label].append(time.monotonic()-begin);assert np.linalg.norm(y-expected)/np.linalg.norm(expected)<=1e-12;sample('coarse_action_benchmark')
 ratio=float(np.median(times['native'])/np.median(times['scipy']));f.close();out.write_text(json.dumps(dict(mesh_tetrahedra=mesh.nelements,coarse_dofs=f.n,coarse_upper_nnz=C.nnz,coarse_input_sha256=f.physical_sha256,library_sha256=sha(D/'support/coarse.dylib'),relative_action_error=error,times_s=times,median_native_to_scipy=ratio,action_speed_gate_passed=ratio<=.8,whole_wall_s=time.monotonic()-start,owned_peak_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss),indent=2)+'\n');print(out.read_text())
if __name__=='__main__':main()
