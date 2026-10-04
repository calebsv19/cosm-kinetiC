"""Evaluate the declared four-interval pressure-force diagnostic; no native adoption."""
import json,hashlib,struct
from pathlib import Path
import numpy as np
from cfd_reference3d_four_interval_projection import interval_trace
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-four-interval'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def native_trace_four(binary,n):
    with binary.open('rb') as f:shape=struct.unpack('=3i',f.read(12))
    if n not in (16,32) or shape!=(2*n,n,n) or binary.stat().st_size!=12+(2*n**3*4+n*n)*8:raise ValueError('unsupported complete archived native field')
    values=np.memmap(binary,dtype='=f8',mode='r',offset=12,shape=(n,n,2*n,4));h=2/n;lo=[round(1.5/h),round(.5/h),round(.5/h)];hi=[round(2.5/h),round(1.5/h),round(1.5/h)]
    averages=[]
    for side in (0,1):
        row=[]
        for d in range(4):
            x=lo[0]-d-1 if side==0 else hi[0]+d
            if x<0 or x>=2*n:raise ValueError('complete fluid stencil required')
            row.append(float(np.mean(values[lo[2]:hi[2],lo[1]:hi[1],x,3])))
        averages.append(row)
    force=interval_trace(averages[0])-interval_trace(averages[1]);return force,averages

def main():
    out=D/'assessment.json';assert not out.exists();support=json.loads((D/'support.json').read_text());assert support['tests_passed']==3 and sha(D/'tests.log')==support['test_log_sha256'] and sha(D/'transforms.json')==support['transforms_sha256']
    for q,h in support['source_sha256'].items():assert sha(R/q)==h==sha(D/'frozen'/q)
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];s=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
        assert s==(R/t['output']).read_text()
    oldpath=R/'build/c3d-cubic-pressure-projection/checkpoint-audit.json';old=json.loads(oldpath.read_text());ap=R/'build/c3d-cubic-pressure-attribution/assessment.json';native=json.loads(ap.read_text());records=[]
    for p in sorted((D/'runs').glob('*/*-receipt.json')):
        rec=json.loads(p.read_text());assert rec['schema']=='physics_sim_c3d_four_interval_receipt_v1' and rec['returncode']==0 and rec['diagnostic_failure'] is None and rec['stop_reason'] is None and rec['wall_s']<180 and rec['peak_observed_rss_bytes']<1024*2**20
        for q,h in rec['artifact_sha256'].items():assert sha(Path(q))==h
        for q,h in rec['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
        assert p.parent.name==hashlib.sha256(json.dumps(rec['source_sha256'],sort_keys=True).encode()).hexdigest() and sha(D/'supervisor-source'/(rec['runner_sha256']+'.py'))==rec['runner_sha256']
        projection_path=Path(rec['command'][rec['command'].index('--output')+1]);row=json.loads(projection_path.read_text());assert row['diagnostic_accepted'] and not row['physical_accuracy_certified'] and len(row['slabs'])==8
        n=row['n'];previous=next(x for x in old['observations'] if x['n']==n);archived=next(x for x in native['records'] if x['n']==n);assert row['input_receipt_sha256']==previous['input_receipt_sha256']==archived['input_reference_receipt_sha256']
        assert sha(Path(row['input_receipt']))==row['input_receipt_sha256'] and sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
        averages=np.array([s['pressure_average_pa'] for s in row['slabs']]);trace=interval_trace(averages[:4])-interval_trace(averages[4:]);raw=row['raw_reference_pressure_force_n'];assert abs(trace-row['native_trace_on_reference_n'])<1e-14
        binary=Path(archived['native_binary']);assert sha(binary)==archived['native_binary_sha256'];four,slabs=native_trace_four(binary,n);reconstruction=trace-raw;field=four-trace;total=four-raw;assert abs(reconstruction+field-total)<1e-15
        ratio=row['trace_reconstruction_relative_difference']/previous['reconstruction_relative_difference'];assert ratio<=.9
        records.append(dict(n=n,receipt=str(p),receipt_sha256=sha(p),input_receipt_sha256=row['input_receipt_sha256'],three_reconstruction_relative_difference=previous['reconstruction_relative_difference'],four_reconstruction_relative_difference=row['trace_reconstruction_relative_difference'],new_old_reconstruction_ratio=ratio,raw_reference_pressure_force_n=raw,projected_four_pressure_force_n=trace,archived_native_three_pressure_force_n=archived['archived_native_pressure_force_n'],archived_native_four_pressure_force_n=four,archived_native_binary=str(binary),archived_native_binary_sha256=sha(binary),native_four_slab_averages_pa=slabs,reconstruction_contribution_relative=reconstruction/raw,field_functional_contribution_relative=field/raw,total_functional_difference_relative=total/raw,complete_pressure_field_error_norm_measured=False))
    assert {r['n'] for r in records}=={16,32}
    result=dict(status='FOUR-INTERVAL DIAGNOSTIC USEFUL; PHYSICAL AND NATIVE ADOPTION OPEN',records=records,source_sha256={**support['source_sha256'],'scripts/assess_cfd_3d_four_interval.py':sha(Path(__file__))},support_sha256=sha(D/'support.json'),predecessor_projection_audit_sha256=sha(oldpath),predecessor_attribution_sha256=sha(ap),selected_for_diagnostic_use=True,native_default_adopted=False,physical_accuracy_certified=False,persistent_goal_complete=False,new_native_solve=False,scope='pressure-force reconstruction on accepted but raw-equilibrium-unqualified reference and matched archived native fields; not a new PDE solution or full field error norm')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(path=str(out),sha256=sha(out),records=records),indent=2))
if __name__=='__main__':main()
