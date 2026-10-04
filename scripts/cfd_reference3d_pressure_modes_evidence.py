"""Immutable diagnostic receipt validation; not numerical flow acceptance."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_pressure_coverage import reserve
from cfd_reference3d_pressure_coarse import reserve as coarse_reserve
from cfd_reference3d_flexible import basis_reservation
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-modes'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(p):
 r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*2**20 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():
  assert sha(p.parent/'source'/q)==h
  current=(R/'scripts'/('cfd_reference3d_pressure_coverage.py' if q=='cfd_reference3d_pressure_modes.py' else q))
  if q=='cfd_reference3d_pressure_modes_probe.py':
   assert (p.parent/'source'/q).read_text().replace('from cfd_reference3d_pressure_modes import','from cfd_reference3d_pressure_coverage import')==current.read_text()
  else:assert sha(current)==h
 assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 assert sha(p.parent/'source/factor.dylib')==r['factor_library_sha256']==r['factor_build']['library_sha256'] and sha(p.parent/'source/factor-build.json')==r['factor_build_record_sha256']
 out=Path(r['command'][r['command'].index('--output')+1]);row=json.loads(out.read_text());assert row['diagnostic_accepted'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and not row['flow_field_published'] and not out.with_suffix('.npz').exists()
 assert row['tetrahedra']<=50000 and row['peak_rss_bytes']<=1800*2**20
 a=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');n=row['condensed_free_dofs'];np_=row['pressure_dofs'];nv=n-np_
 assert a['pressure_diagnostic_reservation_bytes']==reserve(nv,np_) and a['coarse_pressure_reservation_bytes']==coarse_reserve(nv,np_) and a['outer_basis_reservation_bytes']==basis_reservation(n,6)
 assert a['basis_reservation_bytes']==sum(a[k] for k in ('pressure_diagnostic_reservation_bytes','coarse_pressure_reservation_bytes','outer_basis_reservation_bytes')) and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']<=1800*2**20
 assert a['residency_measurement'].startswith('fresh at stage admission') and row['diagnostic']['basis']['first_ten_preserved'] and row['diagnostic']['basis']['columns']<=64 and not row['diagnostic']['complete_spectrum_certified']
 assert row['diagnostic']['projected_relative_skew']<1e-5 and row['diagnostic']['balanced_ten_projected_relative_skew']<1e-5 and min(row['diagnostic']['approximate_mass_schur_projected_eigenvalues'])>0 and min(row['diagnostic']['balanced_ten_projected_eigenvalues'])>0
 return r,row,a
