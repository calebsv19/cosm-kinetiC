"""Known translated scalar profiles: periodic seams versus ambient lateral outflow."""
import copy
import math
from .open_boundaries import layout,face
from open_atmosphere import run,SCHEMA
from .box_qualification import configuration

def request(workers,n,axis,sign,opened=False):
    c=configuration(workers,grid=n,box=False,duration=.3)['fluid']
    c.update(schema=SCHEMA,state=None,steps=[],transport_scheme='muscl_minmod')
    c['buoyancy']['enabled']=False
    c['properties'].update(dynamic_viscosity_pa_s=1e-9,conductivity_w_m_k=0.,tracer_diffusivity_m2_s=0.)
    c['momentum_dt_s']=.0025;c['resource_limits']={'max_cells':32768,'scalar_work_cells':1000000000,'cache_pressure_operator':True}
    if opened:c['boundary_policy']['horizontal']='ambient_open_xy'
    else:
        c['boundary_policy']['vertical']='open_bottom_and_top';c['boundary_policy']['predictor_velocity']='zero_normal_gradient'
    grid=[n,4,4] if axis==0 else [4,n,4];c['grid']=grid
    count=math.prod(grid);c['initial_face_velocity_m_s']=[0.]*layout(c)[0]
    for q in range(count):c['initial_face_velocity_m_s'][axis*count+q]=sign*(.25 if opened else 1.)
    if opened:
        for k in range(grid[2]):
            for t in range(grid[1] if axis==0 else grid[0]):
                c['initial_face_velocity_m_s'][face(grid,axis,n if axis==0 else t,t if axis==0 else n,k,True)]=sign*.25
    def average(i,shift):
        if not opened:
            return 1+.4*math.sin(math.pi/n)/(math.pi/n)*math.cos(2*math.pi*((i+.5)/n-shift))
        center=.75 if sign>0 else .25
        # Independent quadrature of compact cosine-squared packet, zero outside support.
        values=[]
        for t in range(128):
            x=(i+(t+.5)/128)/n-shift;d=(x-center)/.1
            values.append(math.cos(math.pi*d/2)**2 if abs(d)<1 else 0.)
        return math.fsum(values)/128
    profiles=[average((q//(1 if axis==0 else grid[0]))%n,0) for q in range(count)]
    volume=1/count
    c['initial_energy_j']=[v*volume for v in profiles];c['initial_smoke_kg']=[v*volume*.001 for v in profiles]
    c['_average']=average
    return c

def qualify(workers):
    rows=[]
    for axis in (0,1):
        for sign in (-1,1):
            for n in (8,16,32):
                r=request(workers,n,axis,sign);average=r.pop('_average');count=math.prod(r['grid'])
                r['steps']=[{'energy_j':[0.]*count,'smoke_kg':[0.]*count}]*200
                initial=run(r,workers[2]);result=run(dict(r,state=initial['state']),workers[2]);expected=[average((q//(1 if axis==0 else r['grid'][0]))%n,sign)/count for q in range(count)]
                error=math.fsum(abs(a-b) for a,b in zip(expected,result['fields']['energy_j']))/math.fsum(expected)
                rows.append({'axis':axis,'direction':sign,'grid':n,'duration_s':1.,'relative_l1':error,'balance':result['budgets']['energy_j']['balance']})
    errors_pass=all(rows[i+1]['relative_l1']<rows[i]['relative_l1'] for i in range(len(rows)-1) if rows[i+1]['axis']==rows[i]['axis'] and rows[i+1]['direction']==rows[i]['direction'])
    outflows=[]
    for axis in (0,1):
        for sign in (-1,1):
            for n in (16,32,64):
                r=request(workers,n,axis,sign,True);average=r.pop('_average');count=math.prod(r['grid'])
                r['steps']=[{'energy_j':[0.]*count,'smoke_kg':[0.]*count}]*200
                first=run(r,workers[2]);full=run(dict(r,state=first['state']),workers[2])
                half=run(dict(r,steps=r['steps'][:100]),workers[2]);restart=run(dict(r,state=half['state'],steps=r['steps'][100:]),workers[2]);continued=run(dict(r,state=restart['state']),workers[2])
                coord=lambda q:(q//(1 if axis==0 else r['grid'][0]))%n
                exact=[average(coord(q),sign*.25)/count for q in range(count)]
                err=math.fsum(abs(a-b) for a,b in zip(exact,full['fields']['energy_j']))/math.fsum(r['initial_energy_j'])
                opposite=math.fsum(v for q,v in enumerate(full['fields']['energy_j']) if (coord(q)<n//8 if sign>0 else coord(q)>=7*n//8))
                outflows.append({'axis':axis,'direction':sign,'grid':n,'relative_l1_initial_normalized':err,'opposite_edge_amount_j':opposite,'budget':full['budgets'],'full_recovery_exact':full['state']==continued['state']})
    return {'status':'passed' if errors_pass and max(row['relative_l1'] for row in rows if row['grid']==32)<.05 and all(v['full_recovery_exact'] and v['opposite_edge_amount_j']<1e-10 and (v['grid']!=64 or v['relative_l1_initial_normalized']<.15) and v['budget']['energy_j']['outflow']>0 for v in outflows) and all(outflows[i+1]['relative_l1_initial_normalized']<outflows[i]['relative_l1_initial_normalized'] for i in range(len(outflows)-1) if outflows[i+1]['axis']==outflows[i]['axis'] and outflows[i+1]['direction']==outflows[i]['direction']) else 'unqualified','periodic_translation':rows,'ambient_outflow':outflows,'claims':'scalar translation, conservation and exact native recovery; no full traction qualification'}
