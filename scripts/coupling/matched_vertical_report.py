"""Strict matched 2.5-ms full-interval spatial comparisons, with fixed scalar dt."""
import json,math
from pathlib import Path
import numpy as np
from .matched_heat_report import digest
from .flow_time_report import pair,validate as validate64,SOURCE as SOURCE64
from .reconstructed_flow_report import validate_fine
from .flow_spatial_report import load as cubic_load
from .flow_spatial_fields import restrict_extensive,restrict_faces,pack
from .vertical_reference_report import load,restrict_vertical,winds,l2,validate as old_validate,SOURCE
from .isolated_qualification import PROBES

def validate(s):
 if (s['qualification_profile']!='flow_vertical_matched_0025_v1' or s['dt_s']!=.0025 or s['flow_steps']!=1280):raise ValueError('matched vertical flow identity')
 old_validate(dict(s,qualification_profile='flow_vertical_fixed_scalar_v1',dt_s=.005,flow_steps=640))

def comparisons(a,b,sa,sb,n,native_b,grid_b):
 result=pair(a,b,sa,sb,n=n);result.pop('temporal_screens_passed')
 native_winds=[winds(a,[n]*3),winds(native_b,grid_b)]
 norm=l2([v for name in PROBES for v in native_winds[0][name]],[v for name in PROBES for v in native_winds[1][name]])
 result['wind_probes_m_s']=native_winds;result['wind_probe_norm']=norm
 checks=[result['heat']['local_heat_relative_l1'],result['heat']['height_profile_relative_l1'],result['full_mac_velocity']['relative_l2'],norm['relative_l2'],result['full_smoke_relative_l1'],result['peak_rise_relative_difference']]
 result['spatial_screens_passed']=all(v is not None and math.isfinite(v) and v<=.1 for v in checks)
 return result

def baselines(root):
 root=Path(root);i=json.loads((root/'run-identity.json').read_text())
 if digest(i['source'])!=SOURCE or i['source_sha256']!=SOURCE or digest(i['source64'])!=SOURCE64 or i['source64_sha256']!=SOURCE64:raise ValueError('matched source identities')
 summaries=[];arrays=[]
 for n in (64,128):
  run=Path(i['baseline'+str(n)]);s=json.loads((run/'summary.json').read_text())
  (validate64(s,.0025) if n==64 else validate_fine(s,.0025))
  h=s['fields_sha256'] if n==64 else s['fields_sha256']['fields.bin']
  if h!=i['baseline'+str(n)+'_fields_sha256'] or digest(run/'fields.bin')!=h or json.loads((run/'terminal.json').read_text())['returncode']!=0 or 'memory_live_bytes=0 rejected_allocations=0' not in (run/'native.log').read_text():raise ValueError('terminal baseline field/closure identity')
  identity=json.loads((run.parent/'run-identity.json' if n==64 else run/'identity.json').read_text())
  if identity['source_sha256']!=SOURCE64 or identity['probe_sha256']!=s['probe_sha256'] or digest(i['baseline'+str(n)+'_probe'])!=s['probe_sha256']:raise ValueError('baseline probe/source provenance')
  if n==128 and (identity['fine_source_sha256']!=SOURCE or identity['flow_dt_s']!=.0025 or identity['forcing']!='own_accepted_old_time_temperature'):raise ValueError('fine baseline source/forcing/timing')
  a=cubic_load(run/'fields.bin',n)
  if abs(math.fsum(a['energy_j'])-s['energy_j'])>=1e-9 or abs(math.fsum(a['smoke_kg'])-s['smoke_kg'])>=1e-15:raise ValueError('baseline native totals')
  summaries.append(s);arrays.append(a)
 b={k:restrict_extensive(arrays[1][k],64) for k in ('energy_j','smoke_kg')};b['face_velocity_m_s']=pack(restrict_faces(arrays[1]['face_velocity_m_s'],64))
 comparison=comparisons(arrays[0],b,*summaries,64,arrays[1],[128]*3)
 return i,summaries,arrays,comparison

def baseline_report(root):
 i,s,a,c=baselines(root)
 return dict(schema='matched_0025_baseline_v1',identity=i,flow_dt_s=.0025,scalar_dt_s=.00125,heat_threshold_relative=.1,matched64_to128=c,
             local_heat_gate_passed=c['heat']['local_heat_gate_passed'],limitations=['Matched timing 64/128 full 3.2-second baseline.','Does not include the new vertical reference.'])

def report(root):
 root=Path(root);i,s,a,base=baselines(root)
 if (i['grid']!=[128,128,256] or i['flow_dt_s']!=.0025 or i['scalar_dt_s']!=.00125 or i['windows']!=32 or i['window_s']!=.1 or
     digest(root/'matched-vertical-probe')!=i['probe_sha256'] or json.loads((root/'native-terminal.json').read_text())['returncode']!=0 or
     'memory_live_bytes=0 rejected_allocations=0' not in (root/'native.log').read_text()):raise ValueError('new reference identity/terminal closure')
 fine=json.loads((root/'native.json').read_text());validate(fine)
 if digest(root/'fields.bin')!=json.loads((root/'field-identity.json').read_text())['sha256']:raise ValueError('reference field hash')
 full=load(root/'fields.bin',[128,128,256]);restricted=restrict_vertical(full,128)
 for key,tolerance in [('energy_j',1e-9),('smoke_kg',1e-15)]:
  if abs(math.fsum(full[key])-fine[key])>=tolerance or abs(math.fsum(full[key])-math.fsum(restricted[key]))>=tolerance:raise ValueError('new reference totals/restriction')
 directional=comparisons(a[1],restricted,s[1],fine,128,full,[128,128,256])
 coarse={k:restrict_extensive(restricted[k],64) for k in ('energy_j','smoke_kg')};coarse['face_velocity_m_s']=pack(restrict_faces(restricted['face_velocity_m_s'],64))
 cross=comparisons(a[0],coarse,s[0],fine,64,full,[128,128,256])
 return dict(schema='matched_vertical_0025_verdict_v1',identity=i,runs=s+[fine],heat_threshold_relative=.1,domain_m=[1,1,1],heat_region_m=[[.25,.75],[.25,.75],[0,.375]],
             matched64_to128=base,matched128_to_vertical=directional,matched64_to_vertical=cross,
             directional_heat_gate_passed=directional['heat']['local_heat_gate_passed'],directional_screens_passed=directional['spatial_screens_passed'],
             original64_to128_heat_gate_passed=base['heat']['local_heat_gate_passed'],
             limitations=['All three full 3.2-second cases use 2.5 ms flow and 1.25 ms scalar timing.',
                         'The new grid doubles vertical resolution only at fixed 128 horizontal samples; no full three-axis convergence or formal order.',
                         'The saved-state half-step test is separate from this full-from-zero reference.',
                         'No physical calibration, surface response or burning objects qualified.'])
