#!/usr/bin/env python3
"""Inspect immutable completed box field artifacts; never starts a solver.

Produces bounded SI cell samples, XY midpoint pressure/speed previews and a local
HTML inspection report. Reuses the session preview renderer. Derivatives and
absolute force accuracy are not inferred from the preview.
"""
import argparse
import base64
import html
import json
import math
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
from cfd_evidence import experiment_root, seal_bundle, portable_paths, resolve_artifact, verify_bundle
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from preview import image_content
from cfd_run_support import require,save,sha


def inspect(fields,status):
    n=[int(v) for v in status['effective_grid']];spacing=fields['spacing_m']
    velocities=fields['velocity_faces_m_s'];pressures=fields['pressure_pa'];mask=fields['solid_mask']
    def flat(c):return (c[2]*n[1]+c[1])*n[0]+c[0]
    def face(a,c):
        if a==0 and c[0]==n[0]:return fields['outlet_x_velocity_m_s'][c[2]*n[1]+c[1]]
        if not all(0<=c[b]<n[b] for b in range(3)):return 0.
        return velocities[flat(c)][a]
    def cell(c):
        q=flat(c)
        if mask[q]:return [0.,0.,True,0.,0.,0.,None,None,None,None,None]
        v=[]
        for a in range(3):
            upper=c.copy();upper[a]+=1;v.append(.5*(face(a,c)+face(a,upper)))
        return [math.sqrt(sum(x*x for x in v)),0.,False,*v,None,None,None,pressures[q],None]
    k=n[2]//2;cols=min(n[0],64);rows=min(n[1],64)
    samples=[cell([min(n[0]-1,int((i+.5)*n[0]/cols)),min(n[1]-1,int((j+.5)*n[1]/rows)),k])
             for j in reversed(range(rows)) for i in range(cols)]
    statistics={}
    for field,index in [('speed',0),('pressure_pa',9)]:
        values=[row[index] for row in samples if not row[2] and row[index] is not None]
        require(values and all(math.isfinite(v) for v in values),'Finite retained SI slice')
        statistics[field]={'min':min(values),'max':max(values),'finite_samples':len(values)}
    preview=dict(width=cols,height=rows,extent_u_m=n[0]*spacing[0],extent_v_m=n[1]*spacing[1],
                 grid=n,u_axis=0,v_axis=1,samples=samples,statistics=statistics,
                 fields=['speed','dye','solid','vx','vy','vz','pressure_proxy','divergence','vorticity','pressure_pa','shear_stress_pa'],
                 speed_max=statistics['speed']['max'],vectors=False,
                 scope='retained containing-cell pressure and lower/upper-face averaged velocity, cell centres; Y increases upwards; no derivative reconstruction',
                 slice_z_m=(k+.5)*spacing[2])
    probes=[]
    for point in ([.5,1,1],[2,1,1],[3.5,1,1]):
        c=[min(n[a]-1,int(point[a]/spacing[a])) for a in range(3)]
        row=cell(c);probes.append(dict(requested_point_m=point,sample_cell_centre_m=[(c[a]+.5)*spacing[a] for a in range(3)],
                solid=row[2],speed_m_s=row[0],velocity_m_s=row[3:6],pressure_pa=row[9]))
    return preview,probes


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--receipt',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    receipt=json.loads(a.receipt.read_text())
    if (a.receipt.parent/'bundle_manifest.json').exists():verify_bundle(a.receipt.parent)
    require(receipt['status']=='completed_local_stationary_box_scene_and_full_equation_readback','Completed box workflow')
    for q,h in receipt['artifact_sha256'].items():require(sha(a.receipt.parent/q)==h,'Retained artifact identity '+q)
    a.output.mkdir(parents=True,exist_ok=False);summaries=[];cards=[]
    for case in receipt['cases']:
        status=case['status'];name=case['run_id'];artifact=next(x for x in case['result']['artifacts'] if x['path'].endswith('channel_fields.json'))
        require(sha(resolve_artifact(a.receipt.parent,artifact['path']))==artifact['sha256'],'Exact exported field identity')
        fields=json.loads(resolve_artifact(a.receipt.parent,artifact['path']).read_text())['cartesian_fields']
        preview,probes=inspect(fields,status);pngs={}
        for field in ('speed','pressure_pa'):
            preview['field']=field;image=image_content({'preview':preview})
            target=a.output/(name+'-'+field+'.png');target.write_bytes(base64.b64decode(image['data']))
            pngs[field]=dict(path=target.name,sha256=sha(target),display_range=preview['display_range'].copy())
        summary=dict(run_id=name,grid=case['grid'],worker_sha256=receipt['worker_sha256'],field_sha256=artifact['sha256'],
                     snapshot=status['physics'],force=status['boundary_force_budget'],assessment=case['assessment'],
                     slice_statistics=preview['statistics'],slice_z_m=preview['slice_z_m'],probes=probes,images=pngs,
                     sample_scope=preview['scope'],physical_accuracy_certified=False)
        save(a.output/(name+'.json'),summary);summaries.append(summary)
        force=summary['force'];images=[]
        for field,unit in [('speed','m/s'),('pressure_pa','Pa')]:
            low,high=pngs[field]['display_range'];label='Cell speed' if field=='speed' else 'Containing-cell pressure'
            images.append(f'<figure><figcaption>{label}: {low:.6g} to {high:.6g} {unit}</figcaption><img src="{name}-{field}.png" alt="{label} slice"><small>X: 0–4 m, left to right; Y: 0–2 m, bottom to top. Pressure in solids is undefined (magenta); speed solids are grey.</small></figure>')
        cards.append(f'<section><h2>{html.escape(name)} — {case["grid"]}</h2><p>XY cell-centre slice at Z={preview["slice_z_m"]:.6g} m.</p>'+''.join(images)+
                     f'<p>Pressure force X: {force["body_pressure_force_n"][0]:.9g} N; viscous force X: {force["body_viscous_force_n"][0]:.9g} N; inlet pressure: {status["physics"]["pressure_drop_pa"]:.9g} Pa.</p>'+f'<p>Numerical: {case["assessment"]["numerical_status"]}; physical budgets: {case["assessment"]["physical_budget"]["status"]}; absolute force accuracy: not established.</p><p><a href="{name}.json">All three force components, SI probes and provenance</a></p></section>')
    comparison=receipt['comparison'];save(a.output/'comparison.json',comparison)
    changes=comparison['comparisons'][0]['changes'];table=''.join(f'<tr><td>{html.escape(k)}</td><td>{v["coarse"]:.9g}</td><td>{v["fine"]:.9g}</td><td>{100*v["relative_to_fine"]:.5g}%</td></tr>' for k,v in changes.items() if k in ('pressure_drop_pa','physical_strain_dissipation_w','body_pressure_force_n_0','body_viscous_force_n_0'))
    document='''<!doctype html><meta charset="utf-8"><title>Local stationary box CFD inspection</title><style>body{font:16px system-ui;max-width:1000px;margin:2rem auto;padding:0 1rem;line-height:1.5}section{border-top:1px solid #bbb;margin:2rem 0}figure{display:inline-block;width:46%;margin:1%;vertical-align:top}img{width:100%;image-rendering:pixelated;aspect-ratio:2}small{display:block}td,th{padding:.4rem;text-align:right;border-bottom:1px solid #ddd}td:first-child{text-align:left}</style><h1>Stationary box: retained physical fields</h1><p>Aligned 1.5 × 0.5 × 0.5 m body in a 4 × 2 × 2 m duct. Steady incompressible Stokes, no-slip body/walls, natural end pressure tractions, Q=0.008 m³/s, density=1 kg/m³ and viscosity=0.1 Pa s. Bulk body Re=0.03. This is a local source workflow with provisional forces; it does not model transient wakes.</p><p>These images inspect digest-verified exported fields. Complete native equation readback passed separately. Each image uses its own printed range; compare the numeric metrics below. Pressure is not interpolated to the surface.</p>'''+''.join(cards)+'<h2>Resolution sensitivity</h2><p>Relative change uses the finer run as denominator. This is sensitivity, not absolute error.</p><table><tr><th>Metric</th><th>Coarse</th><th>Fine</th><th>Change</th></tr>'+table+'</table><p><a href="comparison.json">Full spatial comparison</a></p>'
    (a.output/'index.html').write_text(document)
    result=dict(status='passed_retained_box_field_inspection',source_sha256={q:sha(ROOT/q) for q in ('scripts/inspect_cfd_box_scene.py','scripts/agent_session/preview.py')},receipt=str(a.receipt.resolve()),receipt_sha256=sha(a.receipt),cases=summaries,
                physical_accuracy_certified=False,artifact_sha256={str(q.relative_to(a.output)):sha(q) for q in a.output.rglob('*') if q.is_file()})
    save(a.output/'receipt.json',result);seal_bundle(a.output);print(json.dumps(dict(status=result['status'],report=str((a.output/'index.html').resolve()))))

if __name__=='__main__':main()
