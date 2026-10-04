"""Diagnostic entry: one small original control or unchanged exact target cube."""
import argparse,json,resource,time
from pathlib import Path
from unittest.mock import patch
import cfd_reference3d_pressure_modes_probe as probe
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_domain_budget import PhaseResourceStopped,enforce_phase
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--case',choices=('small','target'),required=True);ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists();start=time.monotonic()
 try:
  if a.case=='target':
   bundle,geometry=second_normal_tensor_mesh(8.)
   with patch.object(probe,'domain_mesh',return_value=bundle):row=probe.run(length=8.,count=6,split=True,outer_layers=2,factor_library=a.factor_library,domain_mesh_mode='held_l4',coarse_pressure='quadratic',chunk_size=512)
   row['geometry_control']=geometry
  else:row=probe.run(factor_library=a.factor_library,coarse_pressure='quadratic')
  text=json.dumps(row,indent=2)+'\n';enforce_phase('diagnostic_serialization',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
 except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,resource_phase_rejected=e.record);text=json.dumps(row,indent=2)+'\n'
 a.output.write_text(text);enforce_phase('diagnostic_published',resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-start)
 print(json.dumps(dict(phase='diagnostic_published',diagnostic_accepted=row.get('diagnostic_accepted'),wall_s=time.monotonic()-start)),flush=True)
 raise SystemExit(0 if row.get('diagnostic_accepted') else 2)
