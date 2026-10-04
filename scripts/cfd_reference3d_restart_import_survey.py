#!/usr/bin/env python3
"""Import-only optional dependency residency; placeholder cannot run any algorithm."""
import argparse,json,sys,time,resource,types,ctypes as ct,hashlib
from pathlib import Path
begin=time.monotonic();ap=argparse.ArgumentParser();ap.add_argument('--placeholder',action='store_true');ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists()
calls=[]
if a.placeholder:
 module=types.ModuleType('pyamg')
 def unavailable(*args,**kwargs):calls.append(True);raise AssertionError('AMG placeholder is import-only')
 module.smoothed_aggregation_solver=unavailable;sys.modules['pyamg']=module
import cfd_reference3d_restart_tensor_probe
from cfd_reference3d_allocator_pressure import current_rss_bytes
lib=ct.CDLL(None);relief=lib.malloc_zone_pressure_relief;relief.argtypes=(ct.c_void_p,ct.c_size_t);relief.restype=ct.c_size_t
before=current_rss_bytes();relief(None,0);after=current_rss_bytes();peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss;wall=time.monotonic()-begin
assert not calls and peak<1800*1024**2 and wall<180
R=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
row=dict(placeholder=a.placeholder,placeholder_called=False,import_only=True,physical_algorithm_executed=False,current_before_bytes=before,current_after_bytes=after,owned_peak_bytes=peak,wall_s=wall,loaded_modules=len(sys.modules),source_sha256={name:sha(R/name) for name in ('scripts/cfd_reference3d_restart_import_survey.py','scripts/cfd_reference3d_restart_tensor_probe.py','scripts/cfd_reference3d_restart_probe.py','scripts/cfd_reference3d_preconditioner.py','scripts/cfd_fem_reference3d_solenoidal.py')})
a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row))
