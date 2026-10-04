"""One selected finer L4 cube through unchanged full-equation reference engine."""
import argparse,json,hashlib
from pathlib import Path
from unittest.mock import patch
import numpy as np
import cfd_reference3d_complement10_probe as probe
from cfd_reference3d_heldfloor6_mesh import build,SPACINGS
from cfd_reference3d_domain_budget import PhaseResourceStopped

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(geometry,geometry_sha256,factor_library,snapshot):
 if sha(geometry)!=geometry_sha256:raise ValueError('selected geometry archive changed')
 bundle=build(4.,SPACINGS[0]);mesh,lo,hi,axes,n=bundle
 with np.load(geometry,allow_pickle=False) as z:
  for key,value in (('vertices_m',mesh.p),('tetrahedra',mesh.t),('lo',lo),('hi',hi),('axis0',axes[0]),('axis1',axes[1]),('axis2',axes[2])):np.testing.assert_array_equal(z['L4_'+key],value)
  assert int(z['L4_macro_tetrahedra'])==n
 print(json.dumps(dict(phase='selected_geometry_verified',geometry_path=str(geometry),geometry_sha256=geometry_sha256,tetrahedra=mesh.nelements,body_intervals=6,first_strip_m=SPACINGS[0],physical_body_and_domain_preserved=True)),flush=True)
 with patch.object(probe,'domain_mesh',return_value=bundle):row=probe.run(length=4.,count=6,split=True,outer_layers=2,target=1e-10,maxiter=3000,snapshot=snapshot,chunk_size=512,factor_library=factor_library,domain_mesh_mode='held_l4',restart=6,coarse_pressure='quadratic')
 row['geometry_control']=dict(family='held_first_strip_six_interval_redistribution',geometry_sha256=geometry_sha256,geometry_archive=str(geometry),body_intervals=6,first_strip_m=SPACINGS[0],original_macro_partition_claimed=False,uniform_refinement_claimed=False)
 return row
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--geometry-sha256',required=True);ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
 try:row=run(a.geometry,a.geometry_sha256,a.factor_library,a.snapshot)
 except PhaseResourceStopped as e:row=dict(numerically_accepted=False,diagnostic_accepted=False,physical_accuracy_certified=False,numerical_failure_reasons=['resources'],resource_phase_rejected=e.record,iterations=0,final_residual=None)
 a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('numerically_accepted','iterations','final_residual','resource_phase_rejected')}),flush=True);raise SystemExit(0 if row.get('numerically_accepted') else 2)
