"""Seal prospective side-layer geometries without claiming new flow accuracy."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_accuracy_side_layer_survey import build
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-accuracy-side-layer'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 output=D/'checkpoint-audit.json';assert not output.exists()
 support=json.loads((D/'support-receipt.json').read_text());parent=R/'build/c3d-accuracy-first/checkpoint-audit.json';assert sha(parent)==support['predecessor_sha256']=='80512af9c7823ce005232132eae5f7086b47077e957566bc1c4f46c47a69a018'
 assert support['tests_passed']==2 and sha(D/'tests.log')==support['test_log_sha256']
 for q,h in support['source_sha256'].items():assert sha(R/q)==h==sha(D/'frozen'/q)
 # The unchanged parent's frozen solver bundle covers indirect mesh dependencies.
 pr=next((R/'build/c3d-accuracy-first/runs').glob('*/L4-fine-accuracy-first-receipt.json'));p=json.loads(pr.read_text())
 for q,h in p['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(pr.parent/'source'/q)
 survey=json.loads((D/'geometry-survey.json').read_text());assert sha(Path(survey['geometry_archive']))==survey['geometry_sha256'] and survey['wall_s']<600 and survey['peak_rss_bytes']<3072*2**20 and not survey['numerical_factor_attempted'] and not survey['flow_field_published']
 summaries=[]
 with np.load(Path(survey['geometry_archive']),allow_pickle=False) as z:
  for row in survey['rows']:
   length=row['length'];mode=row['mode'];(m,lo,hi,axes,n),_=build(length,mode);prefix=f'L{int(length)}_{mode}_'
   for k,a in [('vertices_m',m.p),('tetrahedra',m.t),('lo',lo),('hi',hi)]+[(f'axis{i}',a) for i,a in enumerate(axes)]:np.testing.assert_array_equal(z[prefix+k],a)
   assert int(z[prefix+'macro_tetrahedra'])==n and row['geometry_invariants_passed'] and row['quality']['all_reflections_and_yz_exchange_preserved'] and row['quality']['physical_boundary_planes_preserved'] and row['quality']['tetrahedra']==m.nelements and row['first_side_normal_spacing_m']==row['previous_first_side_normal_spacing_m']/2
   a=row['original_quality']['global_quality'];b=row['quality']['global_quality'];ratio=b['max_condition']/a['max_condition'];assert ratio>1.9
   summaries.append(dict(length=length,mode=mode,tetrahedra=m.nelements,geometry_invariants_passed=True,within_existing_numerical_mesh_cap=row['within_existing_numerical_mesh_cap'],original_max_condition=a['max_condition'],new_max_condition=b['max_condition'],condition_ratio=ratio,original_worst_shape=a['worst_shape'],new_worst_shape=b['worst_shape'],selected_for_numerical_trial=False,reason='First-layer resolution improves but unbalanced transition worsens conditioning; develop a graded transition before selecting a candidate.'))
 native=json.loads(parent.read_text())['native_hashes_preserved']
 for q,h in native.items():assert sha(R/q)==h
 result=dict(status='GEOMETRY CONTROLS COMPLETE; GRADED TRANSITION NEEDED',persistent_goal_complete=False,tests_passed=2,support_receipt_sha256=sha(D/'support-receipt.json'),predecessor_sha256=sha(parent),survey_sha256=sha(D/'geometry-survey.json'),geometry_sha256=survey['geometry_sha256'],geometry_survey_cap_tetrahedra=75000,original_numerical_mesh_cap_tetrahedra=50000,wall_s=survey['wall_s'],peak_mib=survey['peak_rss_bytes']/2**20,candidates=summaries,source_sha256={**support['source_sha256'],'scripts/audit_cfd_3d_accuracy_side_layer.py':sha(Path(__file__)),'docs/cfd_3d_accuracy_side_layer_checkpoint.md':sha(R/'docs/cfd_3d_accuracy_side_layer_checkpoint.md')},native_hashes_preserved=native,flow_field_published=False,physical_accuracy_certified=False)
 output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(checkpoint=str(output),sha256=sha(output),status=result['status'],survey_wall_s=survey['wall_s'],survey_peak_mib=result['peak_mib'])))
if __name__=='__main__':main()
