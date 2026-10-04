#!/usr/bin/env python3
"""Same-functional pressure-force error attribution, never a full-field norm."""
import hashlib
import argparse
import json
import struct
from pathlib import Path
import numpy as np
from cfd_reference3d_pressure_projection import project

ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-obstacle/refinement-v2'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def native_trace(path,center):
    with path.open('rb') as file:grid=struct.unpack('=3i',file.read(12))
    nx,ny,nz=grid;assert ny==nz;h=2/ny
    values=np.memmap(path,dtype='=f8',mode='r',offset=12,shape=(nz,ny,nx,4))
    lo=[round((center-.5)/h),round(.5/h),round(.5/h)]
    hi=[round((center+.5)/h),round(1.5/h),round(1.5/h)]
    traces=[];averages=[]
    for side in (0,1):
        row=[]
        for depth in range(3):
            x=lo[0]-depth-1 if side==0 else hi[0]+depth
            row.append(float(np.mean(values[lo[2]:hi[2],lo[1]:hi[1],x,3])))
        averages.append(row);traces.append(sum(w*p for w,p in zip((11/6,-7/6,2/6),row)))
    return {'force_n':traces[0]-traces[1],'slab_averages_pa':averages}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--short-reference',default='L4-directional14');args=parser.parse_args()
    matrix=json.loads((ROOT/'build/c3d-obstacle/agent-evidence/qualification.json').read_text())
    rows={r['case']:r for r in matrix['records']}
    extended=json.loads((ROOT/'build/c3d-obstacle/correction-v1/extended40/qualification.json').read_text())
    worker=matrix['worker_sha256'];assert extended['worker_sha256']==worker
    evidence=Path(matrix['evidence_root']);records=[]
    for length,counts in ((4,(8,16,32,48)),(8,(32,40))):
        stem=args.short_reference if length==4 else 'L8-directional14';snapshot=DATA/(stem+'-pressure.npz')
        reference=json.loads((DATA/(stem+'.json')).read_text());rp=reference['pressure_force_n'][0]
        for n in counts:
            output=DATA/f'pressure-{stem}-n{n}.json'
            if output.exists():projection=json.loads(output.read_text());assert projection['snapshot_sha256']==sha(snapshot)
            else:
                projection=project(snapshot,n);output.write_text(json.dumps(projection,indent=2)+'\n')
            if length==4:
                record=rows[f'n{n}'];binary=evidence/f'n{n}.bin'
            elif n==32:record=rows['inlet4'];binary=evidence/'inlet4.bin'
            else:record=extended;binary=ROOT/'build/c3d-obstacle/correction-v1/extended40'/worker/'native.bin'
            assert sha(binary)==record['binary_sha256']
            trace=native_trace(binary,length/2);npforce=record['status']['boundary_force_budget']['body_pressure_force_n'][0]
            assert abs(trace['force_n']-npforce)<1e-12
            projected=projection['native_trace_on_reference_n']
            reconstruction=projected-rp;field=npforce-projected;total=npforce-rp
            assert abs(reconstruction+field-total)<1e-15
            records.append({'length':length,'n':n,'reference_pressure_force_n':rp,
                'reference_projected_native_trace_n':projected,'native_pressure_force_n':npforce,
                'reconstruction_error_n':reconstruction,'field_functional_error_n':field,'total_error_n':total,
                'reconstruction_error_relative':reconstruction/rp,'field_functional_error_relative':field/rp,'total_error_relative':total/rp,
                'native_slab_averages_pa':trace['slab_averages_pa'],
                'reference_slab_averages_pa':[[s['pressure_average_pa'] for s in projection['slabs'] if s['side']==side] for side in (0,1)],
                'projection_sha256':sha(output),'native_binary_sha256':sha(binary),
                'reference_sha256':sha(DATA/(stem+'.json'))})
    result={'schema':'physics_sim_c3d8_pressure_attribution_v1','worker_sha256':worker,'records':records,
        'physical_accuracy_certified':False,
        'scope':'signed force-functional decomposition relative to an independently solved P1 reference; reference qualification is a separate gate, not a full-field error norm'}
    (DATA/('pressure-attribution-'+args.short_reference+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

if __name__=='__main__':main()
