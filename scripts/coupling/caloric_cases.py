"""Native fixed-mixture qualification cases; no chemical or radiative fitting."""
import copy
import math
from . import caloric,gas_mixture as nist,hotgas,hotgas_domain as domain
from .hotgas_domain_cases import config,face


def selected(moles,grid=(4,4,16),length=(.1,.1,.1),gravity=(0.,0.,0.)):
    c=config(grid,length,gravity);c['gas']=caloric.gas_config(c['gas'],moles)
    c['domain']['flux']='lhllc_muscl';return c


def evolve(worker,c,u,steps,source=None,rate=(1,1000000),batch=250):
    source=[0.]*len(u) if source is None else source
    initial=domain.initial(c,worker,u,source,rate);current=initial;receipts=[]
    while current['tick']<steps:
        current,r=domain.advance(c,worker,current,source,min(steps,current['tick']+batch),rate);receipts.append(r)
    return dict(config=c,initial=initial,final=current,source=source,rate=list(rate),receipts=receipts,
                max_conservation_residual=max(abs(v) for r in receipts for v in r['conservation_residual']))


def heating(worker,moles,begin,end=1300.,independent=True):
    c=selected(moles);u=caloric.conserved(moles,1.,begin)*256
    names=('N2','O2','CO2','H2O');mixture={k:v for k,v in zip(names,moles) if v}
    energy=nist.delta_internal(mixture,begin,end) if independent else caloric.properties(moles,end)['energy_j_kg']-caloric.properties(moles,begin)['energy_j_kg']
    source=[0.]*len(u);source[4::5]=[energy/100]*256
    row=evolve(worker,c,u,100,source);temperatures,pressures=hotgas.primitive(c['gas'],row['final']['conserved'])
    row.update(begin_temperature_k=begin,target_temperature_k=end,energy_added_j_kg=energy,
               independent_nist_source=independent,final_temperature_k=temperatures[0],final_pressure_pa=pressures[0])
    return row


def thermal_contact(worker,moles):
    c=selected(moles,grid=(16,4,4),length=(1.,1.,1.));c['domain']['flux']='hllc';c['domain']['faces']=[face('periodic') for _ in range(6)];u=[]
    for q in range(256):
        t=300. if q%16<8 else 1300.;rho=101325/(c['gas']['gas_constant_j_kg_k']*t)
        u+=caloric.conserved(moles,rho,t)
    row=evolve(worker,c,u,200);_,p=hotgas.primitive(c['gas'],row['final']['conserved'])
    row.update(max_density_error=max(abs(a-b) for a,b in zip(u[::5],row['final']['conserved'][::5])),
               max_pressure_error_pa=max(abs(v-101325) for v in p));return row


def transport(worker,moles,n=16):
    c=selected(moles,grid=(n,4,4));c['domain']['faces']=[face('periodic') for _ in range(6)];u=[];p=101325.;base=900.;amplitude=.01;v=10.;r=c['gas']['gas_constant_j_kg_k'];rho0=p/(r*base)
    sinc=math.sin(math.pi/n)/(math.pi/n)
    for q in range(n*16):
        rho=rho0*(1+amplitude*sinc*math.sin(2*math.pi*(q%n+.5)/n));u+=caloric.conserved(moles,rho,p/(rho*r),(v,0.,0.))
    row=evolve(worker,c,u,2000,rate=(1,2000000));out=row['final']['conserved'];_,pressures=hotgas.primitive(c['gas'],out)
    exact=[rho0*(1+amplitude*sinc*math.sin(2*math.pi*((q%n+.5)/n-v*.001/.1))) for q in range(n*16)]
    row.update(relative_density_disturbance_l1=math.fsum(abs(a-b) for a,b in zip(out[::5],exact))/len(exact)/(rho0*amplitude),
               max_pressure_error_pa=max(abs(v-p) for v in pressures),duration_s=.001,
               scope='Smooth frozen-composition entropy transport; not species mixing or diffusion.')
    return row


def hydrostatic(c,t=300.):
    gas=c['gas'];grid=gas['grid'];g=c['domain']['gravity_m_s2'];r=gas['gas_constant_j_kg_k'];avg=1.;u=[]
    for a in range(3):
        half=g[a]*gas['length_m'][a]/grid[a]/(2*r*t);avg*=math.sinh(half)/half if half else 1.
    for q in range(math.prod(grid)):
        stride=1;arg=0.
        for a in range(3):arg+=g[a]*(q//stride%grid[a]+.5)*gas['length_m'][a]/grid[a]/(r*t);stride*=grid[a]
        rho=101325*math.exp(arg)*avg/(r*t);u+=caloric.conserved(gas['caloric_moles'],rho,t)
    return u


def retained_geometry(worker,path,steps,moles):
    import json
    raw=json.loads(path.read_text());old=hotgas.checked(raw);c=copy.deepcopy(old['config']);c['gas']=caloric.gas_config(c['gas'],moles);c['domain']['flux']='lhllc_muscl'
    temperatures,_=hotgas.primitive(old['config']['gas'],old['initial']['conserved']);base=hydrostatic(c);u=[];r=c['gas']['gas_constant_j_kg_k'];selected_cells=[]
    for q,t in enumerate(temperatures):
        pressure=base[5*q]*r*300;rho=pressure/(r*t)
        velocity=[old['initial']['conserved'][5*q+i]/old['initial']['conserved'][5*q] for i in (1,2,3)]
        u+=caloric.conserved(moles,rho,t,velocity)
        if t>600:selected_cells.append(q)
    if any(old.get('source',[0.])):raise ValueError('retained geometry must have no unconverted source')
    row=evolve(worker,c,u,steps,rate=old['rate'],batch=50);v=row['final']['conserved']
    row.update(retained_input_digest=raw['digest'],conversion='Same temperature support and velocity; renewed hydrostatic pressure with mixture R, EOS-consistent density and sensible energy. Original arrays not reinterpreted.',
        selected_region_mean_w_m_s=math.fsum(v[5*q+3]/v[5*q] for q in selected_cells)/len(selected_cells),
        duration_s=steps*float(hotgas.rate(old['rate'])),physical_plume_accuracy_qualified=False)
    return row
