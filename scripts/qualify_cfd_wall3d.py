#!/usr/bin/env python3
"""C3D-7A/B independent spatial/time wall qualification. No package or commit."""
import array
import json
import math
from pathlib import Path
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/c3d-wall';BIN=OUT/'wall3d_test'

def run(n,dt,T,transport,name):
    field=OUT/(name+'.bin');start=time.monotonic();p=subprocess.run([str(BIN),str(n),str(dt),str(T),str(field),str(int(transport))],capture_output=True,text=True,check=True)
    row=json.loads(p.stdout);row['wall_s']=time.monotonic()-start;row['transport']=transport
    assert row['residual']<=1e-11 and row['max_divergence']<1e-8,row
    (OUT/(name+'.json')).write_text(json.dumps(row,indent=2)+'\n');a=array.array('d');a.frombytes(field.read_bytes());return row,a

def rms(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a))

def main():
    OUT.mkdir(parents=True,exist_ok=True);results={}
    for transport,label in ((False,'a'),(True,'b')):
        spatial=[]
        for n in (8,16,32):spatial.append(run(n,.0025,.4,transport,label+'-spatial'+str(n))[0])
        orders=[{k:math.log(a[k]/b[k],2) for k in ('velocity_error','pressure_error','wall_error','dissipation_error')} for a,b in zip(spatial,spatial[1:])]
        finest=spatial[-1];assert finest['velocity_error']<=.03 and finest['pressure_error']<=.03 and finest['wall_error']<=.05 and finest['dissipation_error']<=.05 and finest['energy_imbalance']<=.05
        # Report the coarse onset; the finest pair is the asymptotic screen.
        assert all(v>=1.8 for v in orders[-1].values()),orders
        temporal=[];fields=[]
        for dt in (.04,.02,.01,.005,.0025):
            row,f=run(16,dt,.4,transport,label+'-time'+str(dt));temporal.append(row);fields.append(f)
        m=3*16**3-2*16**2
        changes=[{'velocity':rms(a[:m],b[:m]),'pressure':rms(a[m:],b[m:])} for a,b in zip(fields,fields[1:])]
        time_orders=[{k:math.log(a[k]/b[k],2) for k in a} for a,b in zip(changes,changes[1:])]
        assert all(v>=1.8 for row in time_orders[-2:] for v in row.values()),time_orders
        results[label]={'spatial':spatial,'spatial_orders':orders,'temporal':temporal,'temporal_changes':changes,'temporal_orders':time_orders,'passed':True}
        (OUT/('qualification-'+label+'.json')).write_text(json.dumps(results[label],indent=2)+'\n')
        print(json.dumps({label:{'passed':True,'spatial_orders':orders,'temporal_orders':time_orders}}),flush=True)
    (OUT/'qualification-ab.json').write_text(json.dumps({'passed':True,'deliveries':results},indent=2)+'\n')
if __name__=='__main__':main()
