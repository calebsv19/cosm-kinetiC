import json,hashlib
from pathlib import Path
import numpy as np
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt
from cfd_reference3d_exact_prediction import decode_chunks
from cfd_reference3d_shared_factor import storage_sha
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-exact-prediction';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='b73af22e43c23420c734c8aa353ffe256ed5a9b42011f86bca88e473c04c590f'
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'representation-runs').glob('*/*-receipt.json'));r,row=parent.frozen(p);assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
 assert row['diagnostic_accepted'] and row['storage_potential_accepted'] and not row['symbolic_factor_attempted'] and not row['numeric_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and row['original_physical_inputs_preserved_bitwise'] and row['original_full_mixed_action_preserved_bitwise']
 assert r['wall_s']<180 and max(row['peak_rss_bytes'],r['peak_observed_rss_bytes'])<1800*2**20 and row['tetrahedra']==33216 and not any(q.get('phase') in ('workspace_symbolic_ready','workspace_numeric_ready','solve','publication_complete') for q in r['progress'])
 pp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));_,old=verify_receipt(pp);assert row['identity']==old['identity']
 m=row['exact_prediction'];assert m['coefficients']==22411620 and m['coefficient_batch_cap']==262144 and m['original_value_sha256']==m['decoded_value_sha256'] and m['predictor_float_sha256']==old['preconditioner']['rounded_values_sha256'] and m['bitwise_roundtrip_verified'] and m['original_values_preserved_bitwise']
 assert m['original_joint_bytes']==m['physical_double_bytes']+m['original_pc_float_bytes'] and m['encoded_joint_bytes']==m['predictor_bytes']+m['word_correction_bytes']==m['encoded_bytes'] and m['saved_joint_bytes']==m['original_joint_bytes']-m['encoded_joint_bytes']>=32*2**20 and m['storage_potential_accepted']
 archive=Path(r['command'][r['command'].index('--prediction')+1]);assert row['exact_prediction_published'] and archive.exists()
 with np.load(archive,allow_pickle=False) as z:
  predictor=z['coefficient_predictor_f32'];delta=z['coefficient_word_corrections_i32'];assert predictor.dtype==np.dtype('float32') and delta.dtype==np.dtype('int32') and predictor.shape==delta.shape==(22411620,)
  assert storage_sha(predictor)==m['predictor_float_sha256'] and storage_sha(predictor,delta)==m['encoded_array_sha256'] and int(delta.min())==m['minimum_word_correction']>=np.iinfo(np.int32).min and int(delta.max())==m['maximum_word_correction']<=np.iinfo(np.int32).max
  h=hashlib.sha256();h.update(str(((len(predictor),),np.dtype('float64').str)).encode())
  for a in decode_chunks(predictor,delta):assert np.all(np.isfinite(a));h.update(memoryview(a).cast('B'))
  assert h.hexdigest()==m['decoded_value_sha256']
 sources=list((R/'scripts').glob('*exact_prediction*.py'))+[R/'tests/test_cfd_reference3d_exact_prediction.py']+list((R/'docs').glob('cfd_3d_exact_prediction*.md'))
 result=dict(status='PROGRESS: complete exact shared-predictor joint storage potential85.494MiB; encoded action/factor proof next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256={str(p):sha(p),str(pp):sha(pp)},native_hashes_preserved=native,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),actual_representation=m,storage_potential_accepted=True,numerical_storage_adapter_adopted=False,numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),joint_saved_mib=m['saved_joint_bytes']/2**20)))
if __name__=='__main__':main()
