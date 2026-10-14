#!/usr/bin/env python3
"""Three-stage boundary transport, isolated plume and inert-box qualification."""
import argparse,json,os
from pathlib import Path
from coupling.boundary_verification import qualify
from coupling.isolated_qualification import coupled,frozen,write
from coupling.box_qualification import spatial
from surface_sources.growth_fire_v1 import sealed

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--stage',choices=('boundary','plume'),required=True);a=p.parse_args();root=a.output.resolve();root.mkdir(parents=True,exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(root/'native-evidence');workers=(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker)
    if a.stage=='boundary':
        result=qualify(workers);write(root/'summary.json',sealed(result));print(result['status'],flush=True);return
    hosts={};rows={};reports={}
    for name,box in (('no-box',False),('box',True)):
        d=root/name;d.mkdir();hosts[name],rows[name],reports[name]=coupled(workers,d,box);print(name+' coupled complete',flush=True)
    frames=[row['source']['frame'] for row in rows['no-box'][1:]];schedule=hosts['no-box'].plan(rows['no-box'])['schedule'];write(root/'matched-source-frames.json',frames)
    refinements={};cases={}
    for name,box in (('no-box',False),('box',True)):
        results=[];checks=[]
        for n in (8,16,32):
            d=root/('matched-'+name+'-'+str(n));d.mkdir();result,report=frozen(workers,d,frames,schedule,n,box);results.append(result);checks.append(report);print(name+' mesh '+str(n)+' complete',flush=True)
        refinements[name]=spatial(results,checks);cases[name]=checks
    # Independent domain-size sensitivity, same .0625 m cells and same world source.
    d=root/'domain-expanded-24';d.mkdir();larger,report=frozen(workers,d,frames,schedule,24,False,1.5)
    reference=json.loads((root/'matched-no-box-16/fluid.json').read_text())
    delta={}
    for key in ('energy_j','smoke_kg'):
        cropped=[larger['fields'][key][(k*24+j+4)*24+i+4] for k in range(16) for j in range(16) for i in range(16)]
        delta[key]=sum(abs(a-b) for a,b in zip(reference['fields'][key],cropped))/sum(reference['fields'][key])
    qualified=all(v['scalar_differences_decrease'] and v['probe_differences_decrease'] and all(pair[k]['relative_l1']<.1 for k in ('energy_j','smoke_kg')) for v in refinements.values() for pair in v['common_partition_pairs'][1:]) and all(v<.1 for v in delta.values())
    summary=sealed({'schema':'fire_fluid_isolated_plume_qualification/v1','status':'passed' if qualified else 'accuracy_unqualified','coupled':reports,'refinement':refinements,'matched_cases':cases,'domain_size_relative_l1':delta,'domain_size_checks':report,'threshold_relative_l1':.1,'thermal_feedback':False,'physical_calibration':False})
    write(root/'summary.json',summary);print(json.dumps({'status':summary['status'],'refinement':refinements,'domain_size_relative_l1':delta},indent=2),flush=True)
if __name__=='__main__':main()
