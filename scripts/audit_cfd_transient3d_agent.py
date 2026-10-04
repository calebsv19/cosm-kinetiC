#!/usr/bin/env python3
"""Independent C3D-7 exported-field and multi-run agent audit; no completion inference."""
import array
import hashlib
import json
import math
from pathlib import Path
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
import audit_cfd_transient3d_numerics as independent
from verify_cfd_transient3d_agent import cases
from service import Service


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def rms(a,b):
    assert len(a)==len(b)
    return math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a))


def packed(record):
    artifact=next(a for a in record['result']['artifacts'] if a['path'].endswith('channel_fields.json'))
    path=Path(artifact['path']);assert sha(path)==artifact['sha256']
    document=json.loads(path.read_text());f=document['cartesian_fields']
    n=record['request']['grid'];nx,ny,nz=n
    periodic=f['periodic_axes'][0]
    lengths=record['status']['physics']['dimensions_m']
    assert max(abs(h-L/count) for h,L,count in zip(f['spacing_m'],lengths,n))<1e-15
    assert f['periodic_axes']==[periodic,False,False]
    assert periodic==(record['request']['scene_id'].startswith(('cfd_wall_stokes_3d','cfd_wall_transport_3d')))
    assert len(f['velocity_faces_m_s'])==nx*ny*nz and len(f['pressure_pa'])==nx*ny*nz
    if not periodic:assert len(f['outlet_x_velocity_m_s'])==ny*nz
    values=array.array('d')
    # Convert the declared lower-face artifact, including its explicit upper open
    # X face, into physical component vectors. Known Y/Z wall normals are omitted.
    for a in range(3):
        for k in range(nz if a!=2 else nz-1):
            for j in range(ny if a!=1 else ny-1):
                for i in range(nx+(a==0 and not periodic)):
                    c=[i,j,k]
                    if a:c[a]+=1
                    if i==nx:values.append(f['outlet_x_velocity_m_s'][k*ny+j])
                    else:values.append(f['velocity_faces_m_s'][(c[2]*ny+c[1])*nx+c[0]][a])
    for q,row in enumerate(f['velocity_faces_m_s']):
        j=(q//nx)%ny;k=q//(nx*ny)
        if j==0:assert row[1]==0
        if k==0:assert row[2]==0
    values.extend(f['pressure_pa'])
    assert all(math.isfinite(x) for x in values)
    native=array.array('d');native.frombytes(Path(record['native_field']).read_bytes())
    assert len(values)==len(native)
    # Different optimization levels may vary roundoff, never physical fields.
    delta=max(abs(x-y) for x,y in zip(values,native));assert delta<1e-10,(record['case'],delta)
    return values,periodic,artifact,delta


def physical_observations(record, values, periodic):
    state=record['status'];n=record['request']['grid'];length=state['physics']['dimensions_m']
    h=[L/count for L,count in zip(length,n)];volume=math.prod(h)
    rho=state['physics']['density_kg_m3'];mu=state['physics']['dynamic_viscosity_pa_s']
    shapes,offsets,m,face=independent.layout(values,n,periodic)
    kinetic=0.
    for a,shape in enumerate(shapes):
        for k in range(shape[2]):
            for j in range(shape[1]):
                for i in range(shape[0]):
                    weight=.5 if not periodic and a==0 and i in (0,n[0]) else 1
                    u=values[offsets[a]+(k*shape[1]+j)*shape[0]+i]
                    kinetic+=.5*rho*volume*weight*u*u
    assert abs(kinetic-state['energy_budget']['kinetic_energy_j'])<1e-12
    def center(a,c):
        high=c.copy();high[a]+=1
        return .5*(face(a,*c)+face(a,*high))
    actual=[]
    for b in (1,2):
        for side in (0,1):
            tangents=[0] if state['solve_mode']=='pressure_startup' else [a for a in range(3) if a!=b]
            sums=[0. for _ in tangents];count=0
            for k in range(n[2]):
                for j in range(n[1]):
                    for i in range(n[0]):
                        c=[i,j,k]
                        if c[b]!=(n[b]-1 if side else 0):continue
                        count+=1
                        for slot,a in enumerate(tangents):
                            shear=-2*mu*center(a,c)/h[b]
                            if state['solve_mode']=='pressure_startup':sums[slot]+=-shear*volume/h[b]
                            else:sums[slot]+=shear*shear
            actual.extend(sums if state['solve_mode']=='pressure_startup' else [math.sqrt(x/count) for x in sums])
    key='wall_drag_n' if state['solve_mode']=='pressure_startup' else 'wall_tangential_shear_rms_pa'
    assert len(actual)==len(state['boundary_force_budget'][key])
    assert max(abs(a-b) for a,b in zip(actual,state['boundary_force_budget'][key]))<1e-12
    # The retained independently qualified native fixture must report the same
    # physical reference diagnostics for the identical accepted fields/time.
    path=Path(record['native_field'])
    stem=path.stem
    if stem.startswith('c-'):stem=stem.rsplit('-t',1)[0]
    native=(json.loads(path.with_name('qualification-phase.json').read_text())['final'] if stem=='b-phase32'
            else json.loads(path.with_name(stem+'.json').read_text()))
    if isinstance(native,list):native=next(r for r in native if abs(r['time']-state['simulation_time'])<1e-9)
    ref=state['qualification']['transient_reference_gate']
    for agent,numerical in [('max_wall_relative_error','wall_error'),('energy_relative_error','dissipation_error'),('energy_imbalance','energy_imbalance')]:
        assert abs(ref[agent]-native[numerical])<1e-10,(record['case'],agent)
    energy=state['energy_budget']
    balance=energy['physical_boundary_power_w']+energy['body_force_power_w']-energy['transport_power_w']-energy['physical_strain_dissipation_w']-energy['kinetic_energy_rate_w']
    assert abs(balance-energy['residual_w'])<1e-12
    return {'kinetic_energy_j_from_faces':kinetic,'wall_observations_from_faces':actual,
            'energy_budget_terms_consistent':True,'native_reference_diagnostics_match':True}


def metric(row,key):return row['status']['qualification']['transient_reference_gate'][key]


def main():
    base=ROOT/'build/c3d-wall/agent-evidence'
    evidence=json.loads((base/'qualification.json').read_text())
    records={r['case']:r for r in evidence['records']}
    assert set(records)=={r[0] for r in cases()}
    checkpoint=json.loads((ROOT/'build/c3d-wall/numerical-checkpoint.json').read_text())
    assert checkpoint['numerical_matrix_passed']
    for file,digest in checkpoint['source_sha256'].items():assert sha(ROOT/file)==digest
    original=json.loads((ROOT/'build/c3d/completion-audit.json').read_text())
    for file,preserved in checkpoint['preserved_baseline_sources'].items():
        assert preserved is True
        assert sha(ROOT/file)==original['source_sha256'][file]
    opened=json.loads((ROOT/'build/c3d-open/completion-audit.json').read_text())
    for file in ('include/app/cfd_open3d.h','src/app/cfd_open3d.c','src/app/cfd_open3d_observation.c'):
        assert sha(ROOT/file)==opened['source_sha256'][file]
    storage=Path(evidence['evidence_root'])/'readback';storage.mkdir(exist_ok=True)
    independent.OUT=storage
    readbacks=[];fields={}
    for name,record in records.items():
        assert record['worker_sha256']==evidence['worker_sha256']
        for artifact in record['result']['artifacts']:assert sha(Path(artifact['path']))==artifact['sha256']
        values,periodic,artifact,delta=packed(record);fields[name]=values
        physical=physical_observations(record,values,periodic)
        file=name+'.bin';(storage/file).write_bytes(values.tobytes())
        state=record['status'];ref=state['qualification']['transient_reference_gate']
        n=record['request']['grid'];length=state['physics']['dimensions_m']
        row={'time':state['simulation_time'],'velocity_error':ref['velocity_relative_l2'],
             'pressure_error':ref['pressure_relative_error'],'n':n[1]}
        if state['solve_mode']=='pressure_startup':
            Q=state['health']['volume_flux_m3_s'];err=ref['volume_flow_relative_error']
            # Recover the continuous reference from a separately derived series,
            # rather than solving its value from an unsigned relative error.
            t=state['simulation_time'];H=length[1];W=length[2]
            mean=H*H/12*(1-192*H/(math.pi**5*W)*sum(math.tanh((2*j+1)*math.pi*W/(2*H))/(2*j+1)**5 for j in range(1024)))
            G=.1*.008/(H*W*mean);flow=.008
            for a in range(128):
                for b in range(128):
                    ky=(2*a+1)*math.pi/H;kz=(2*b+1)*math.pi/W;wave=ky*ky+kz*kz
                    c=16*G/(.1*math.pi**2*(2*a+1)*(2*b+1)*wave)
                    flow-=c*math.exp(-.1*wave*t)*4/(ky*kz)
            assert abs(abs(Q/flow-1)-err)<1e-10
            row.update(grid=n,length=length[0],flow=Q,reference_flow=flow)
            audit=independent.startup(file,row)
        else:audit=independent.manufactured(file,row,length[0],periodic)
        readbacks.append({'case':name,'artifact_sha256':artifact['sha256'],'native_field_sha256':sha(Path(record['native_field'])),
                          'max_agent_native_difference':delta,'physical_observations':physical,'independent_readback':audit})
    spatial={}
    for label in ('a','b','co'):
        group=[records[label+'-spatial'+str(n)] for n in (8,16,32)]
        orders=[{k:math.log(metric(a,k)/metric(b,k),2) for k in
                 ('velocity_relative_l2','pressure_relative_error','max_wall_relative_error','energy_relative_error')}
                for a,b in zip(group,group[1:])]
        assert min(orders[-1].values())>=1.8
        assert group[-1]['assessment']['reference_accuracy']['status']=='passed'
        spatial[label]=orders
    temporal={}
    for label in ('a','b','co'):
        dts=(.04,.02,.01,.005,.0025)
        names=[label+'-time'+str(dt) if dt!=(.005 if label=='co' else .0025) else label+'-spatial16' for dt in dts]
        n=records[names[0]]['request']['grid'];m=len(fields[names[0]])-math.prod(n)
        changes=[{'velocity':rms(fields[a][:m],fields[b][:m]),'pressure':rms(fields[a][m:],fields[b][m:])} for a,b in zip(names,names[1:])]
        orders=[{k:math.log(a[k]/b[k],2) for k in a} for a,b in zip(changes,changes[1:])]
        assert min(v for row in orders[-2:] for v in row.values())>=1.8
        temporal[label]={'differences':changes,'orders':orders}
    for t in (.5,2):
        names=['c-time'+str(dt)+'-t'+format(t,'.6g') for dt in (.05,.025,.0125,.00625,.003125)]
        m=len(fields[names[0]])-32*16*16
        changes=[{'velocity':rms(fields[a][:m],fields[b][:m]),'pressure':rms(fields[a][m:],fields[b][m:])} for a,b in zip(names,names[1:])]
        orders=[math.log(a['velocity']/b['velocity'],2) for a,b in zip(changes,changes[1:])]
        assert min(orders[-2:])>=1.8 and max(row['pressure'] for row in changes)<1e-10
        temporal['startup-'+str(t)]={'differences':changes,'velocity_orders':orders,'pressure_order':'not_applicable_exact_linear_pressure'}
    startup_spatial={}
    for t in (.5,2,12):
        names=['c-spatial'+str(n)+'-t'+format(t,'.6g') for n in (8,16,32)]
        orders=[math.log(metric(records[a],'velocity_relative_l2')/metric(records[b],'velocity_relative_l2'),2) for a,b in zip(names,names[1:])]
        assert orders[-1]>=1.8 and records[names[-1]]['assessment']['reference_accuracy']['status']=='passed'
        startup_spatial[str(t)]=orders
    def upstream(name):
        record=records[name];f=fields[name];n=record['request']['grid']
        _,_,m,face=independent.layout(f,n,False)
        vel=[];pressure=[]
        for a in range(3):
            for k in range(n[2]-(a==2)):
                for j in range(n[1]-(a==1)):
                    for i in range(16,49 if a==0 else 48):
                        vel.append(face(a,i,j+(a==1),k+(a==2)))
        for k in range(n[2]):
            for j in range(n[1]):
                for i in range(16,48):pressure.append(f[m+(k*n[1]+j)*n[0]+i])
        return vel,pressure
    outlet=[]
    for a,b in zip(('co-spatial32','co-outlet6'),('co-outlet6','co-outlet8')):
        va,pa=upstream(a);vb,pb=upstream(b)
        changes={'velocity':rms(va,vb)/math.sqrt(sum(v*v for v in va)/len(va)),'pressure':rms(pa,pb)/.01}
        for key in ('kinetic_energy_j','physical_strain_dissipation_w'):
            ea=records[a]['status']['energy_budget'][key]/records[a]['status']['physics']['dimensions_m'][0]
            eb=records[b]['status']['energy_budget'][key]/records[b]['status']['physics']['dimensions_m'][0]
            changes[key+'_per_length']=abs(eb/ea-1)
        assert max(changes.values())<=.01
        outlet.append({'from':a,'to':b,**changes})
    startup_outlet=[]
    for t in (.5,2):
        names=['c-spatial32-t'+format(t,'.6g')]+['c-outlet'+str(L)+'-t'+format(t,'.6g') for L in (6,8)]
        for a,b in zip(names,names[1:]):
            ra,rb=records[a]['status'],records[b]['status'];la=ra['physics']['dimensions_m'][0];lb=rb['physics']['dimensions_m'][0]
            change={'flow':abs(rb['health']['volume_flux_m3_s']/ra['health']['volume_flux_m3_s']-1)}
            for key in ('kinetic_energy_j','physical_strain_dissipation_w'):
                change[key+'_per_length']=abs((rb['energy_budget'][key]/lb)/(ra['energy_budget'][key]/la)-1)
            change['wall_per_length']=max(abs((y/lb)/(x/la)-1) for x,y in zip(ra['boundary_force_budget']['wall_drag_n'],rb['boundary_force_budget']['wall_drag_n']))
            assert max(change.values())<=.01
            startup_outlet.append({'from':a,'to':b,**change})
    phase=records['b-phase32']['status']['qualification']['harmonic_reference']
    native_phase=json.loads((ROOT/'build/c3d-wall/qualification-phase.json').read_text())['results']
    for key in ('velocity','pressure'):
        for metric_name in ('offset','harmonic_amplitude','amplitude_relative_error','phase_error_degrees'):
            actual=phase[key][metric_name];expected=native_phase[key+'_amplitude'][metric_name]
            if metric_name=='phase_error_degrees':expected=abs(expected)
            assert abs(actual-expected)<1e-8,(key,metric_name,actual,expected)
    service=Service(evidence['evidence_root'])
    comparisons={label:service.run_compare([records[label+'-spatial'+str(n)]['run_id'] for n in (8,16,32)]) for label in ('a','b','co')}
    final_flow=records['c-spatial32-t12']['status']['health']['volume_flux_m3_s']
    assert abs(final_flow/.008-1)<.01
    output={'schema':'physics_sim_c3d_transient_agent_readback_v1','agent_readback_passed':True,'goal_complete':False,
            'worker_sha256':evidence['worker_sha256'],'records':readbacks,'spatial_orders':spatial,'temporal':temporal,
            'startup_spatial_orders':startup_spatial,'open_outlet_changes':outlet,'startup_outlet_changes':startup_outlet,
            'harmonic_reference':phase,'agent_comparisons':comparisons,'startup_steady_flow_error':abs(final_flow/.008-1),
            'remaining':'full requirement, control/cost, regression and source audit'}
    encoded=json.dumps(output,indent=2)+'\n'
    (Path(evidence['evidence_root'])/'readback-audit.json').write_text(encoded)
    (base/'readback-audit.json').write_text(encoded)
    print(json.dumps({'agent_readback_passed':True,'cases':len(records),'goal_complete':False}))


if __name__=='__main__':main()
