#!/usr/bin/env python3
"""Audit symmetric reference robustness and pressure attribution without certification by selection."""
import argparse
import json
from pathlib import Path
from audit_cfd_obstacle3d import ROOT, DATA, sha, check, error, reference_gate, screen, distance

PROOF=DATA/'refinement-v2'


def load(path):return json.loads(path.read_text())

def ref(name):return load(PROOF/(name+'.json'))


def compare(first,second):
    checks={}
    for key in ('pressure_force_n','raw_symmetric_viscous_force_n','physical_dissipation_w','inlet_pressure_pa'):
        a,b=first[key],second[key]
        if isinstance(a,list):a,b=a[0],b[0]
        checks[key]=check(error(b,a),.01)
    return {'status':'passed' if all(c['status']=='passed' for c in checks.values()) else 'failed','checks':checks}


def reaction(row):
    return check(error(row['pressure_force_n'][0]+row['raw_symmetric_viscous_force_n'][0],row['reaction_force_n'][0]),.01)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--require-qualified',action='store_true');args=parser.parse_args()
    previous=load(PROOF/'predecessor-completion-audit.json')
    matrix=load(DATA/'agent-evidence/qualification.json');rows={r['case']:r for r in matrix['records']}
    extended=load(DATA/'correction-v1/extended40/qualification.json');extended['case']='extended40'
    worker=matrix['worker_sha256']
    assert worker==extended['worker_sha256']==previous['worker_sha256']
    assert sha(ROOT/'build/cfd-optimized/physics_sim_session_worker')==worker
    runtime={p:h for p,h in previous['source_sha256'].items() if p.startswith(('src/','include/'))}
    before=load(PROOF/'predecessor-source-sha256.json')
    assert all(sha(ROOT/p)==h for p,h in {**runtime,**before}.items())
    for record in [*rows.values(),extended]:
        assert record['readback_max_difference']==0 and not record['assessment']['physical_accuracy_certified']
        for artifact in record['result']['artifacts']:assert sha(Path(artifact['path']))==artifact['sha256']
    inherited=load(DATA/'correction-v1/predecessor-qualification.json')
    old_rows={r['case']:r for r in inherited['records']}
    assert all(r['binary_sha256']==old_rows[name]['binary_sha256'] for name,r in rows.items())
    receipts={};reference_evidence={};failures={}
    for path in sorted(PROOF.glob('*-receipt.json')):
        receipt=load(path);stem=path.name.removesuffix('-receipt.json')
        assert sha(PROOF/(stem+'.log'))==receipt['log_sha256']
        frozen=Path(receipt['command'][1]).parent
        assert all(sha(frozen/name)==h for name,h in receipt['source_sha256'].items())
        assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
        receipts[stem]=receipt
        if receipt['returncode']:
            failures[stem]=receipt;continue
        assert sha(PROOF/(stem+'.json'))==receipt['output_sha256']
        assert sha(PROOF/(stem+'-pressure.npz'))==receipt['pressure_sha256']
        row=ref(stem);assert row['tetrahedra']<=50000 and row['free_relative_residual']<1e-8 and row['flux_error']<1e-8
        assert row['peak_rss_bytes']<=1800*1024**2 and receipt['wall_s']<=181
        if row['body']:
            for t in row['traction_checks']:
                assert max(abs(a-b) for a,b in zip(t['raw_viscous_force_n'],row['raw_symmetric_viscous_force_n']))<1e-11
                assert max(abs(a-b) for a,b in zip(t['pressure_force_n'],row['pressure_force_n']))<1e-11
        reference_evidence[stem]={'tetrahedra':row['tetrahedra'],'pressure_force_n':row['pressure_force_n'],
            'raw_viscous_force_n':row['raw_symmetric_viscous_force_n'],'reaction_force_n':row['reaction_force_n'],
            'stabilization_reaction_n':row['stabilization_reaction_n'],'pressure_drop_pa':row['inlet_pressure_pa'],
            'dissipation_w':row['physical_dissipation_w'],'true_residual':row['free_relative_residual'],
            'wall_s':row['wall_s'],'peak_rss_bytes':row['peak_rss_bytes'],'reaction_check':reaction(row) if row['body'] else None}
    # Same gamma0 system after the pressure-preconditioner/true-residual change.
    equivalence={}
    for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','physical_dissipation_w','inlet_pressure_pa'):
        a,b=ref('L8-directional14')[key],ref('L8-directional14-true-stop')[key]
        if isinstance(a,list):a,b=a[0],b[0]
        equivalence[key]=abs(b/a-1)
    assert max(equivalence.values())<1e-7
    # Retained independently evaluated Fourier pressure for L4, same fluid/Q.
    fourier=load(DATA/'correction-v1/predecessor-completion-audit.json')['empty_continuous_pressure_pa']
    calibrations={}
    for length,stem in ((4,'L4-matched-empty10'),(8,'L8-directional-empty10')):
        row=ref(stem);pin=fourier*length/4
        calibrations[str(length)]={'pressure':check(error(row['inlet_pressure_pa'],pin),.01),
            'dissipation':check(error(row['physical_dissipation_w'],pin*.008),.01)}
    candidates={}
    for length,last,penultimate,normal_base in ((4,'L4-matched14','L4-matched12','L4-matched12'),
                                               (8,'L8-directional14','L8-directional12','L8-directional12')):
        final=ref(last);core=reference_gate(ref(penultimate),final)
        robustness=compare(ref(normal_base),ref(f'L{length}-normal-split'))
        penalized=compare(ref(f'L{length}-normal-unsplit-gd01'),ref(f'L{length}-normal-split-gd01'))
        gamma_sensitivity=compare(ref(f'L{length}-divergence-gd01'),ref(f'L{length}-divergence-gd1'))
        tangential=final['traction_checks'][1]['tangential_viscous_force_n'][0]
        normal=final['traction_checks'][1]['normal_viscous_force_n'][0]
        candidates[str(length)]={'status':'not_established','core_refinement_gate':core,
            'genuine_normal_refinement':robustness,'gamma_mu_genuine_normal_refinement':penalized,
            'grad_div_parameter_sensitivity':gamma_sensitivity,'gamma10mu_reaction':reaction(ref(f'L{length}-divergence-gd1')),
            'normal_viscous_force_n':normal,'tangential_viscous_force_n':tangential,
            'normal_fraction_of_raw_viscous':abs(normal/final['raw_symmetric_viscous_force_n'][0]),
            'normal_trace_scope':'Exact stationary flat no-slip incompressible normal derivative is zero; discrete P2/P1 normal stress is retained as approximation error, not projected away'}
    # Keep the misleading stable-grid controls and their failures visible.
    failed_allocations={}
    for length in (4,8):
        failed_allocations[f'L{length}-gap3']=reference_gate(ref(f'L{length}-body16'),ref(f'L{length}-body18'))
        failed_allocations[f'L{length}-gap4-long4']=reference_gate(ref(f'L{length}-gap4-body12'),ref(f'L{length}-gap4-body14'))
        failed_allocations[f'L{length}-directional']=reference_gate(ref(f'L{length}-directional12'),ref(f'L{length}-directional14'))
    old_short_sensitivity=compare(ref('L4-gap4-body12-long6'),ref('L4-matched12'))
    longitudinal_control=compare(ref('L8-directional12'),ref('L8-gap4-body12-long6'))
    attribution=ref('pressure-attribution-L4-matched14')
    for row in attribution['records']:
        projection=PROOF/f"pressure-{'L4-matched14' if row['length']==4 else 'L8-directional14'}-n{row['n']}.json"
        assert sha(projection)==row['projection_sha256']
        assert abs(row['reconstruction_error_n']+row['field_functional_error_n']-row['total_error_n'])<1e-15
    screens=[screen(rows[f'n{n}'],ref('L4-matched14')) for n in (8,16,32,48)]
    long_screen=screen(extended,ref('L8-directional14'))
    gradient=load(DATA/'native-empty32-calibration.json')['status']['physics']['pressure_drop_pa']/4
    downstream=distance(rows['outlet6'],rows['outlet8'],gradient);upstream=distance(rows['outlet8'],rows['inlet4'],gradient)
    refinements={}
    for name in ('pressure_force','viscous_force','total_force','dissipation'):
        values=[r['physical'][name]['value'] for r in screens]
        refinements[name]={'errors':values,'status':'passed' if all(a>b for a,b in zip(values,values[1:])) else 'failed',
                           'scope':'declared four short grids against unresolved matched reference'}
    blockers=[]
    for length in (4,8):
        candidate=candidates[str(length)]
        background=old_short_sensitivity if length==4 else longitudinal_control
        eligible=(candidate['core_refinement_gate']['status']=='passed' and candidate['genuine_normal_refinement']['status']=='passed'
                  and background['status']=='passed' and all(c['status']=='passed' for c in calibrations[str(length)].values()))
        candidate['status']='qualified' if eligible else 'not_established'
        if candidate['genuine_normal_refinement']['status']!='passed':blockers.append(f'L{length} reference genuine X-normal force refinement')
        if background['status']!='passed':blockers.append(f'L{length} reference background/node-policy sensitivity')
        if candidate['core_refinement_gate']['status']!='passed':blockers.append(f'L{length} reference component/reaction refinement')
    for label,row in [('short_n48',screens[-1]),('long_n40',long_screen)]:
        for name,c in row['physical'].items():
            if c['status']!='passed':blockers.append(f'native {label} {name} screening')
    if any(m['status']!='passed' for m in refinements.values()):blockers.append('native declared component refinement')
    numerical=[screen(record,ref('L4-matched14')) for record in [*rows.values(),extended]]
    if any(row['numerical_status']!='passed' for row in numerical):blockers.append('native numerical conservation')
    if downstream['status']!='passed' or upstream['status']!='passed':blockers.append('native boundary distance')
    qualified=not blockers
    log_hashes={}
    for name,count in [('measurement-tests.log',1),('native-pressure-tests.log',1)]:
        path=PROOF/name;text=path.read_text();assert text.count('\nOK\n')==count and 'FAILED' not in text;log_hashes[name]=sha(path)
    sanitizer=PROOF/'native-pressure-sanitize.log';text=sanitizer.read_text();assert 'runtime error:' not in text and 'ERROR: AddressSanitizer' not in text and 'Assertion failed' not in text
    json.loads(text);log_hashes[sanitizer.name]=sha(sanitizer)
    for name in ('measurement-known-answer','native-pressure-known-answer','native-pressure-cusp-control'):assert ref(name)['passed']
    sources=set(runtime)|set(before)
    sources.update(str(p.relative_to(ROOT)) for p in (ROOT/'scripts').glob('*cfd*3d*.py'))
    sources.update(['scripts/assess_cfd_obstacle3d_pressure_attribution.py','scripts/cfd_reference3d_mesh.py',
        'scripts/cfd_reference3d_traction.py','scripts/cfd_reference3d_pressure_projection.py',
        'tests/test_cfd_reference3d_measurement.py','tests/test_cfd_obstacle3d_pressure_trace.py',
        'tests/cfd_obstacle3d_pressure_trace_probe.c','make/rules-tools.mk',
        'docs/cfd_obstacle3d_reference_refinement_goal.md','docs/cfd_obstacle3d_reference_refinement_checkpoint.md',
        'docs/current_truth.md','docs/cfd_3d_completion.md','docs/agent_session.md','docs/README.md'])
    output={'schema':'physics_sim_c3d8_reference_refinement_audit_v1','status':'qualified' if qualified else 'implemented_reference_robustness_unqualified',
        'physical_accuracy_certified':qualified,'worker_sha256':worker,'native_runtime_sources_byte_preserved':len(runtime),
        'native_original_seven_fields_byte_preserved':True,'native_agent_readbacks_inherited_exact_worker':8,
        'maximum_readback_difference':0,'reference_candidates':candidates,'failed_allocation_controls':failed_allocations,
        'old_short_X_node_sensitivity':old_short_sensitivity,'longitudinal_body_control':longitudinal_control,
        'empty_continuous_calibration':calibrations,'solver_preconditioner_true_stop_equivalence':equivalence,
        'reference_numerical_solves':reference_evidence,'failed_reference_receipts':failures,
        'pressure_attribution':attribution,'native_short_screens':screens,'native_long_screen':long_screen,
        'downstream_distance':downstream,'upstream_distance':upstream,
        'blocking_gates':blockers,'native_refinement_screens':refinements,'native_numerical_screens':numerical,
        'cost':{'reference_attempts':len(receipts),'successful_numerical_references':len(reference_evidence),
                'sum_reference_child_wall_s':sum(r['wall_s'] for r in receipts.values()),
                'maximum_reference_observed_rss_bytes':max(r['peak_observed_rss_bytes'] for r in receipts.values()),
                'resource_scope':'per-child bounded local measurements, some overlap; summed cost is not elapsed time or a benchmark',
                'mesh_cap':50000,'rss_cap_bytes':1800*1024**2,'wall_cap_s':180},
        'limitations':['Stokes stationary aligned cube only; no new native fluid behavior or field accuracy certificate',
          'Force-functional pressure decomposition depends on an unresolved P1 reference, not a complete field-error norm',
          'Known square-root pressure is an analytic regularity counterexample, not the measured physical corner exponent',
          'Tiny true residual and small final force changes do not qualify a normal-refinement-sensitive reference',
          'No native adaptive grids, STL/moving bodies, inertial/turbulent wakes, commit, package or adoption'],
        'source_sha256':{p:sha(ROOT/p) for p in sorted(sources)},'test_log_sha256':log_hashes,
        'evidence_sha256':{str(p.relative_to(DATA)):sha(p) for p in PROOF.rglob('*') if p.is_file() and p.name!='completion-audit.json'}}
    (PROOF/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    (DATA/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({k:output[k] for k in ('status','physical_accuracy_certified','blocking_gates','cost')}))
    if args.require_qualified and not qualified:raise SystemExit(1)

if __name__=='__main__':main()
