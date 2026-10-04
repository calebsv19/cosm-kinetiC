#!/usr/bin/env python3
"""Independent one-period harmonic projections for C3D-7B velocity/physical Pa."""
import csv
import json
import math
from pathlib import Path
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/c3d-wall'
def main():
    file=OUT/'b-phase32.csv';start=time.monotonic()
    p=subprocess.run([str(OUT/'wall3d_test'),'32','.02','1',str(OUT/'b-phase32.bin'),'1',str(file)],capture_output=True,text=True,check=True)
    row=json.loads(p.stdout);data=list(csv.DictReader(file.open()));n=len(data);assert n==50
    results={}
    for key,expected,phase in [('velocity_amplitude',.2,0),('pressure_amplitude',.01,.3)]:
        s=2/n*sum(float(x[key])*math.sin(2*math.pi*float(x['time'])) for x in data)
        c=2/n*sum(float(x[key])*math.cos(2*math.pi*float(x['time'])) for x in data)
        amplitude=math.hypot(s,c);actual_phase=math.atan2(c,s) if key=='velocity_amplitude' else math.atan2(-s,c)
        error=math.atan2(math.sin(actual_phase-phase),math.cos(actual_phase-phase))
        results[key]={'offset':sum(float(x[key]) for x in data)/n,'harmonic_amplitude':amplitude,'amplitude_relative_error':abs(amplitude/expected-1),'phase_error_degrees':error*180/math.pi}
        assert abs(error)*180/math.pi<=1 and abs(amplitude/expected-1)<=.03,results
    result={'schema':'physics_sim_c3d_wall_phase_v1','passed':True,'grid':[32,32,32],'dt':.02,'period_s':1,'method':'one-period discrete sine/cosine projection with independent physical pressure mode','results':results,'final':row,'wall_s':time.monotonic()-start}
    (OUT/'qualification-phase.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(results))
if __name__=='__main__':main()
