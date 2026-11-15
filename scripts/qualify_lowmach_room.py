#!/usr/bin/env python3
"""Bounded CP2C room bridge checks; no remote work or long room campaign."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time

R,CP,T0,P0=287.,1005.,300.,101325.

def read(path):
    lines=path.read_text().splitlines();header=lines[0].split()
    assert header[0]=='CP2C1'
    n,vent,obstacle=map(int,header[1:4]);gravity,p0,t=map(float.fromhex,header[4:7]);steps,cells,nf=map(int,header[7:])
    assert cells==n**3 and len(lines)==1+cells+nf
    state=[list(map(float.fromhex,x.split())) for x in lines[1:1+cells]]
    velocity=[float.fromhex(x) for x in lines[1+cells:]]
    # Independent physical-coordinate mask, matched to the existing room's SI surfaces.
    mask=[];positions=[]
    for k in range(n):
        for j in range(n):
            for i in range(n):
                x,y,z=[(a+.5)/n for a in (i,j,k)]
                aperture=.375<=y<.625 and .375<=z<.875
                box=obstacle and .5<=x<.625 and .25<=y<.375 and z<.25
                mask.append(x<.0625 or y<.0625 or y>=.9375 or z>=.9375 or (x>=.9375 and not aperture) or box)
                positions.append((x,y,z))
    faces=[];cell_vel=[[0.,0.,0.] for _ in state];div=[0.]*cells
    for q in range(cells):
        if mask[q]:
            assert state[q]==[0.,0.,0.]
            continue
        coord=[q%n,q//n%n,q//(n*n)]
        for axis,stride in enumerate((1,n,n*n)):
            other=q+stride if coord[axis]+1<n else -1
            if other>=0 and mask[other] or other<0 and not (vent and axis==0):continue
            faces.append((q,other,axis))
    assert len(faces)==nf
    for u,(q,other,a) in zip(velocity,faces):
        cell_vel[q][a]+=.5*u;div[q]+=n*u
        if other>=0:cell_vel[other][a]+=.5*u;div[other]-=n*u
    dv=1/cells;gas=[q for q in range(cells) if not mask[q]]
    rho=[x[0] for x in state];enthalpy=[x[1] for x in state]
    temp=[enthalpy[q]/CP/rho[q] if not mask[q] else 0 for q in range(cells)]
    anomaly=[max(enthalpy[q]-CP*T0*rho[q],0)*dv for q in range(cells)]
    energy=math.fsum(anomaly)
    outlet=[f for f,(_,b,_) in enumerate(faces) if b<0]
    assert len(outlet)/n**2==(0.125 if vent else 0)
    assert len(gas)/cells==.7216796875
    metrics=dict(n=n,time_s=t,steps=steps,p0_pa=p0,volume_m3=len(gas)*dv,
        mass_kg=math.fsum(rho)*dv,enthalpy_j=math.fsum(enthalpy)*dv,
        max_temperature_k=max(temp),min_temperature_k=min(temp[q] for q in gas),
        max_speed_component_m_s=max(map(abs,velocity)),
        thermal_energy_above_ambient_j=energy,
        thermal_centroid_z_m=math.fsum(a*xyz[2] for a,xyz in zip(anomaly,positions))/energy if energy>1e-8 else None,
        thermal_mean_w_m_s=math.fsum(a*v[2] for a,v in zip(anomaly,cell_vel))/energy if energy>1e-8 else None,
        vent_volume_out_m3_s=math.fsum(max(velocity[f],0)/n**2 for f in outlet),
        vent_volume_in_m3_s=math.fsum(max(-velocity[f],0)/n**2 for f in outlet),
        eos_error=max(abs(enthalpy[q]*R/CP/p0-1) for q in gas),
        vent_area_m2=len(outlet)/n**2,
        vertical_temperature_k=[math.fsum(temp[q] for q in gas if q//(n*n)==k)/sum(q//(n*n)==k for q in gas) for k in range(15*n//16)])
    return metrics,dict(rho=rho,enthalpy=enthalpy,velocity=velocity,faces=faces,mask=mask,div=div,positions=positions,temp=temp)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--strict',type=Path,required=True)
    p.add_argument('--sanitized',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);start=time.monotonic()
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0');results={};states={}
    def run(name,mode,n,dt,steps,worker=None,restart=None):
        assert time.monotonic()-start<600,'bounded qualification wall budget'
        dest=a.output/(name+'.state');hist=a.output/(name+'.csv')
        cmd=[str((worker or a.strict).resolve()),mode,str(n),str(dt),str(steps),str(dest.resolve()),str(hist.resolve())]
        if restart:cmd.append(str(restart.resolve()))
        done=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=180)
        (a.output/(name+'.log')).write_text(done.stdout+done.stderr)
        assert done.returncode==0,(name,done.stdout,done.stderr)
        metrics,fields=read(dest);receipts=list(csv.DictReader(hist.open()))
        initial_mass=P0/(R*T0)*metrics['volume_m3'];initial_h=CP/R*P0*metrics['volume_m3']
        sums={key:math.fsum(float(row[key]) for row in receipts) for key in ('heat_j','outward_mass_kg','outward_enthalpy_j','pressure_work_j')}
        if not restart:
            mass_error=metrics['mass_kg']-initial_mass+sums['outward_mass_kg']
            enthalpy_error=metrics['enthalpy_j']-initial_h+sums['outward_enthalpy_j']-sums['heat_j']-sums['pressure_work_j']
            assert abs(mass_error)<1e-10 and abs(enthalpy_error)<1e-6,(name,mass_error,enthalpy_error)
            metrics.update(independent_mass_residual_kg=mass_error,independent_enthalpy_residual_j=enthalpy_error)
        metrics.update(sums)
        metrics['max_divergence_error']=max((float(r['divergence_error']) for r in receipts),default=0)
        metrics['max_mach']=max((float(r['max_mach']) for r in receipts),default=0)
        metrics['maximum_pressure_iterations']=max((int(r['iterations']) for r in receipts),default=0)
        metrics['sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
        assert metrics['eos_error']<1e-8 and metrics['max_divergence_error']<1e-8
        results[name]=metrics;states[name]=fields
        (a.output/'progress.json').write_text(json.dumps(dict(last=name,elapsed_s=time.monotonic()-start)))
        print(name,json.dumps(metrics),flush=True)
        return dest
    for worker in (a.strict,a.sanitized):
        check=subprocess.run([str(worker.resolve().parent/'native_test')],text=True,capture_output=True,env=env,timeout=60)
        (a.output/(worker.parent.name+'-native.log')).write_text(check.stdout+check.stderr)
        assert check.returncode==0,check.stderr
    run('cold','cold',16,.005,20)
    assert results['cold']['max_speed_component_m_s']==0
    run('sealed','sealed',16,.005,20)
    expected=P0+R/(CP-R)*1e5*.1
    assert abs(results['sealed']['p0_pa']-expected)<1e-8
    assert abs(results['sealed']['max_temperature_k']-(T0+1e5*.1/(P0/(R*T0)*(CP-R))))<1e-8
    run('vent','vent',16,.005,20)
    run('vent-half','vent',16,.0025,40)
    rate=1e5*R/(CP*P0);analytic_rho=P0/(R*T0)*math.exp(-rate*.1)
    errors=[]
    for name,dt in [('vent',.005),('vent-half',.0025)]:
        fields=states[name];gas=[r for r,m in zip(fields['rho'],fields['mask']) if not m]
        discrete=P0/(R*T0)*(1-rate*dt)**round(.1/dt)
        assert max(abs(r-discrete) for r in gas)<1e-10
        error=max(abs(r-analytic_rho) for r in gas);results[name]['analytic_density_error']=error;errors.append(error)
        assert abs(results[name]['vent_volume_out_m3_s']-results[name]['vent_volume_in_m3_s']-rate*results[name]['volume_m3'])<1e-10
    assert .45<errors[1]/errors[0]<.55
    final=run('plume','plume',16,.005,40)
    run('nogravity','nogravity',16,.005,40)
    assert results['plume']['thermal_mean_w_m_s']>results['nogravity']['thermal_mean_w_m_s']
    assert results['plume']['thermal_centroid_z_m']>results['nogravity']['thermal_centroid_z_m']
    run('plume-half','plume',16,.0025,80)
    run('plume-fine','plume',32,.005,40)
    first=run('restart-prefix','plume',16,.005,20)
    last=run('restart-final','plume',16,.005,20,restart=first)
    assert final.read_bytes()==last.read_bytes(),'actual fresh-process restart mismatch'
    prefix=run('strict-prefix','plume',16,.005,5)
    san=run('sanitized-prefix','plume',16,.005,5,worker=a.sanitized)
    assert prefix.read_bytes()==san.read_bytes(),'strict/sanitized mismatch'
    # Bad restart is refused before an output state is created.
    bad=a.output/'invalid.state';bad.write_text(first.read_text().replace('CP2C1','WRONG',1))
    badout=a.output/'invalid-output.state'
    rejected=subprocess.run([str(a.strict.resolve()),'plume','16','.005','1',str(badout),str(a.output/'invalid.csv'),str(bad)],capture_output=True,text=True,timeout=10)
    assert rejected.returncode!=0 and not badout.exists()
    for name in ('plume-half','plume-fine'):
        results[name]['relative_change_from_base']={k:abs(results[name][k]/results['plume'][k]-1) for k in ('thermal_mean_w_m_s','thermal_centroid_z_m','max_temperature_k')}
    report=dict(status='first_checks_passed',elapsed_s=time.monotonic()-start,cases=results,
        restart_exact=True,sanitizer_exact=True,invalid_restart_refused=True,
        limits=['constant cp ideal air','inviscid slip walls','first-order donor transport and momentum','small-height reduced hydrostat','no combustion, diffusion, radiation, wall heat or long campaign','mesh/time sensitivity measured; no convergence or physical accuracy claim'])
    (a.output/'assessment.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
