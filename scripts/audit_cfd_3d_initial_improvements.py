#!/usr/bin/env python3
"""Read retained initial improvements evidence and write only its separate audit."""
import hashlib
import json
import math
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-initial-improvements'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    roots=list((DATA/'traction-v1').glob('*/L4-unsplit.json'))
    assert len(roots)==1, 'select one frozen reference experiment explicitly if more are added'
    refroot=roots[0].parent;rows={};receipts=[]
    for length in (4,8):
        for kind in ('unsplit','split'):
            stem=f'L{length}-{kind}';row=json.loads((refroot/(stem+'.json')).read_text())
            receipt=json.loads((refroot/(stem+'-receipt.json')).read_text());receipts.append(receipt)
            assert receipt['returncode']==0 and receipt['stop_reason'] is None
            assert receipt['mesh_cap']==50000 and row['tetrahedra']<=50000
            assert receipt['rss_cap_bytes']==1800*1024**2 and receipt['peak_observed_rss_bytes']<=receipt['rss_cap_bytes']
            assert receipt['wall_cap_s']==180 and receipt['wall_s']<=181
            assert row['free_relative_residual']<1e-8 and row['flux_error']<1e-8
            for name,h in receipt['source_sha256'].items():assert sha(refroot/'source'/name)==h
            for path,h in receipt['artifact_sha256'].items():assert sha(Path(path))==h
            prior=('L4-matched12' if length==4 else 'L8-directional12') if kind=='unsplit' else f'L{length}-normal-split'
            old=json.loads((ROOT/'build/c3d-obstacle/refinement-v2'/(prior+'.json')).read_text())
            equivalence={}
            for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w'):
                a,b=old[key],row[key]
                if isinstance(a,list):a,b=a[0],b[0]
                equivalence[key]=abs(b/a-1)
            assert max(equivalence.values())<1e-7
            d=row['consistency_diagnostics']
            errors=list(d['normal_divergence_identity_error_n'])
            for face in d['faces']:errors.extend(a-b for a,b in zip(face['normal_load_n'],face['divergence_load_n']))
            errors.extend(lift['integration_by_parts_identity_error_n'] for lift in d['volume_lifts'])
            assert max(abs(x) for x in errors)<1e-8*abs(row['reaction_force_n'][0])
            for lift in d['volume_lifts']:assert abs(lift['vector_laplacian_load_n']/row['reaction_force_n'][0]-1)<1e-7
            rows[stem]={'tetrahedra':row['tetrahedra'],'true_residual':row['free_relative_residual'],
                        'predecessor_equivalence':equivalence,'pressure_force_n':row['pressure_force_n'][0],
                        'raw_viscous_force_n':row['raw_symmetric_viscous_force_n'][0],
                        'weak_total_force_n':row['reaction_force_n'][0],'diagnostics':d}
    changes={}
    for length in (4,8):
        a,b=rows[f'L{length}-unsplit'],rows[f'L{length}-split']
        changes[str(length)]={key:abs(b[key]/a[key]-1) for key in ('pressure_force_n','raw_viscous_force_n','weak_total_force_n')}
        normal_change=b['diagnostics']['normal_load_n'][0]-a['diagnostics']['normal_load_n'][0]
        changes[str(length)]['normal_fraction_of_viscous_change']=normal_change/(b['raw_viscous_force_n']-a['raw_viscous_force_n'])
    logs={p.name:sha(p) for p in (DATA/'logs').glob('*.log')}
    for name in ['reference-tests.log','agent-tests.log','cartesian-regression.log','transient-regression.log','refined-regression.log']:
        text=(DATA/'logs'/name).read_text();assert '\nOK\n' in text and 'FAILED' not in text
    native=(DATA/'logs/native-tests.log').read_text()
    assert native.count('cached solve, cancellation and exact-cap cleanup passed')==2
    assert 'ERROR: AddressSanitizer' not in native and 'runtime error:' not in native
    solved=[json.loads(line) for line in (DATA/'logs/reference-tests.log').read_text().splitlines() if line.startswith('{"passed"')][0]
    assert solved['passed'] and len(solved['records'])==12
    native_worker=DATA/'native-build/physics_sim_session_worker';worker=sha(native_worker)
    readback=DATA/'agent-evidence'/worker/'initial-readback.json';record=json.loads(readback.read_text())
    assert record['worker_sha256']==worker and record['assessment']['numerical_status']=='passed'
    assert not record['assessment']['physical_accuracy_certified']
    for artifact in record['result']['artifacts']:assert sha(Path(artifact['path']))==artifact['sha256']
    artifact=next(a for a in record['result']['artifacts'] if a['path'].endswith('channel_fields.json'))
    fields=json.loads(Path(artifact['path']).read_text())['cartesian_fields']
    matrix=json.loads((ROOT/'build/c3d-obstacle/agent-evidence/qualification.json').read_text())
    binary=Path(matrix['evidence_root'])/'n16.bin';raw=binary.read_bytes();nx,ny,nz=struct.unpack_from('=3i',raw)
    previous=next(r for r in matrix['records'] if r['case']=='n16');assert sha(binary)==previous['binary_sha256']
    values=struct.unpack_from('='+str(4*nx*ny*nz+ny*nz)+'d',raw,12);errors=[]
    for q,(v,p,solid) in enumerate(zip(fields['velocity_faces_m_s'],fields['pressure_pa'],fields['solid_mask'])):
        errors.extend(abs(v[a]-values[4*q+a]) for a in range(3))
        if solid:assert p is None and math.isnan(values[4*q+3])
        else:errors.append(abs(p-values[4*q+3]))
    errors.extend(abs(x-y) for x,y in zip(fields['outlet_x_velocity_m_s'],values[4*nx*ny*nz:]))
    assert max(errors)==0
    historical=ROOT/'build/cfd-optimized/physics_sim_session_worker'
    assert sha(historical)=='c2a234ff0238f12b64c739ccbb1f6304e2ff3244613b125eb453fd40d08caf64'
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,h in baseline['hashes'].items() if sha(ROOT/p)!=h]
    assert all(not p.startswith(('include/','src/')) or p in ('src/app/cfd_obstacle3d.c','src/app/cfd_obstacle3d_observation.c') for p in changed)
    sources=changed+['scripts/cfd_reference3d_consistency.py','scripts/run_cfd_reference3d_consistency.py',
        'scripts/audit_cfd_3d_initial_improvements.py','tests/test_cfd_reference3d_solved.py',
        'tests/test_cfd_reference3d_consistency.py','docs/cfd_3d_initial_improvements_goal.md','docs/cfd_3d_initial_improvements_checkpoint.md']
    audit={'schema':'physics_sim_c3d_initial_improvements_v1','initial_slice_complete':True,'physical_accuracy_certified':False,
        'status':'reliability_and_diagnostics_verified_obstacle_accuracy_unqualified','worker_sha256':worker,
        'historical_worker_preserved':sha(historical),'solved_reference_fixture':solved,'reference_records':rows,
        'normal_refinement_changes':changes,'reference_cost':{'sum_child_wall_s':sum(r['wall_s'] for r in receipts),
            'maximum_observed_rss_bytes':max(r['peak_observed_rss_bytes'] for r in receipts)},
        'native_field_readback':{'predecessor_case':'n16','values_checked':len(errors),'maximum_difference':max(errors),'current_run':str(readback)},
        'test_logs_sha256':logs,'changed_preexisting_files':changed,'source_sha256':{p:sha(ROOT/p) for p in sorted(set(sources))},
        'reference_receipts':[str(refroot/(f'L{length}-{kind}-receipt.json')) for length in (4,8) for kind in ('unsplit','split')],
        'limits':['No original obstacle reference/component acceptance gate changed',
                 'Stable weak total load does not certify pressure/viscous components',
                 'No maximum-grid latency, Desktop interaction/package, restart or general geometry proof',
                 'Reference diagnostics are not substitutes for raw physical traction'],
        'committed':False,'packaged':False,'canonical_changed':False}
    (DATA/'completion-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','initial_slice_complete','physical_accuracy_certified','reference_cost','native_field_readback')}))

if __name__=='__main__':main()
