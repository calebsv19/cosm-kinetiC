"""Accuracy-lane receipt proof; physical convergence remains separately assessed."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_accuracy_budget import RSS_CAP,WALL_CAP,numerical_failure_reasons
from cfd_reference3d_accuracy_floor import GEOMETRY_SHA
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(path):
    path=Path(path).resolve();r=json.loads(path.read_text());directory=path.parent
    assert r['schema']=='physics_sim_c3d_accuracy_first_receipt_v1' and r['rss_cap_bytes']==RSS_CAP and r['wall_cap_s']==WALL_CAP and r['mesh_cap']==50000 and r['linear_iteration_cap']==3000
    assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['wall_s']<WALL_CAP and r['peak_observed_rss_bytes']<RSS_CAP
    for n,h in r['artifact_sha256'].items():assert sha(Path(n))==h
    for n,h in r['source_sha256'].items():assert sha(directory/'source'/n)==h==sha(R/'scripts'/n)
    assert directory.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(directory.parent.parent/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    output=Path(r['command'][r['command'].index('--output')+1]);snapshot=Path(r['command'][r['command'].index('--snapshot')+1]);geometry=Path(r['command'][r['command'].index('--geometry')+1]);geometry=geometry if geometry.is_absolute() else R/geometry;assert sha(geometry)==GEOMETRY_SHA
    row=json.loads(output.read_text());assert row['numerically_accepted'] and not numerical_failure_reasons(row) and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10
    assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
    pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['constant_pressure_direction_retained'] and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5
    a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assembled=next(x for x in r['progress'] if x.get('phase')=='assembled');n=assembled['reduced_free_dofs'];np_=assembled['condensed_pressure_dofs'];total=basis_reservation(n,6)+reserve(n-np_,np_)
    assert a['numeric_stage_admitted'] and a['rss_cap_bytes']==RSS_CAP and a['basis_reservation_bytes']==total and a['reserve_bytes']==32*2**20 and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))<=RSS_CAP
    with np.load(snapshot,allow_pickle=False) as z,np.load(geometry,allow_pickle=False) as g:
        prefix='L4_' if row['length']==4. else 'L8_';assert bool(z['numerically_accepted']);np.testing.assert_array_equal(z['vertices_m'],g[prefix+'vertices_m']);np.testing.assert_array_equal(z['tetrahedra'],g[prefix+'tetrahedra']);assert np.all(np.isfinite(z['velocity_coefficients'])) and np.all(np.isfinite(z['pressure_coefficients']))
    return r,row

def physical_comparison(coarse,fine,kind):
    def relative(a,b):return float(abs(a/b-1))
    forces={k:relative(fine[k][0],coarse[k][0]) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')}
    scalars={k:relative(fine[k],coarse[k]) for k in ('inlet_pressure_pa','physical_dissipation_w')}
    p=np.asarray(fine['pressure_force_n']);v=np.asarray(fine['raw_symmetric_viscous_force_n']);r=np.asarray(fine['reaction_force_n']);raw=float(np.linalg.norm(p+v-r)/np.linalg.norm(r))
    return dict(comparison_kind=kind,separate_force_relative_changes=forces,inlet_pressure_dissipation_relative_changes=scalars,force_change_gate=.01,force_change_passed=all(x<=.01 for x in forces.values()),raw_surface_reaction_relative_mismatch=raw,raw_equilibrium_gate=.01,raw_equilibrium_passed=raw<=.01,scalar_refinement_gate=.01 if kind=='same_domain_refinement' else None,scalar_refinement_passed=all(x<=.01 for x in scalars.values()) if kind=='same_domain_refinement' else None,domain_scalars_are_length_dependent=kind=='domain_sensitivity',physical_accuracy_certified=False)
if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('receipt',type=Path);a=ap.parse_args();r,row=verify(a.receipt);print(json.dumps(dict(receipt=str(a.receipt),wall_s=r['wall_s'],iterations=row['iterations'],full_residual=row['final_residual'],owned_mib=row['peak_rss_bytes']/2**20)))
