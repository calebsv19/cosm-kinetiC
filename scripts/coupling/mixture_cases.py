"""Bounded evolving-species qualification fixtures, not reacting plume models."""
import math
from . import mixture as m

def config(n=16,diffusion=0.,conductivity=0.):
    return dict(grid=[n,4,4],length_m=[1.,1.,1.],diffusion_m2_s=diffusion,conductivity_w_m_k=conductivity)

def evolve(worker,c,u,steps,source=None,rate=(1,1000000),batch=100):
    source=[0.]*len(u) if source is None else source
    initial=m.initial(c,worker,u,source,rate);current=initial;receipts=[]
    while current['tick']<steps:
        current,r=m.advance(c,worker,current,source,min(current['tick']+batch,steps),rate);receipts.append(r)
    return dict(config=c,source=source,rate=list(rate),initial=initial,final=current,receipts=receipts)

def advect(worker,n=16):
    c=config(n);u=[];velocity=10.;amplitude=.02;p=101325.;sinc=math.sin(math.pi/n)/(math.pi/n)
    for q in range(n*16):
        a=amplitude*sinc*math.sin(2*math.pi*(q%n+.5)/n)
        y=[.65-a,.2-.5*a,.03+.25*a,.02+.25*a,.1+a];r=m.properties(y,300.)['R'];t=p/r
        u+=m.conserved(y,1.,t,(velocity,0.,0.))
    row=evolve(worker,c,u,1000);out=row['final']['conserved'];pressure=[];errors=[];energy_errors=[]
    for q in range(n*16):
        state=m.primitive(out[10*q:10*q+10]);pressure.append(state['pressure'])
        change=amplitude*sinc*math.sin(2*math.pi*((q%n+.5)/n-velocity*.001))
        y=[.65-change,.2-.5*change,.03+.25*change,.02+.25*change,.1+change]
        t=p/m.properties(y,300.)['R'];exact=m.conserved(y,1.,t,(velocity,0.,0.))
        errors.append(abs(state['y'][4]-y[4]));energy_errors.append(abs(out[10*q+4]-exact[4]))
    mean_energy=math.fsum(u[4::10])/(n*16)
    energy_scale=math.fsum(abs(e-mean_energy) for e in u[4::10])/(n*16)
    row.update(relative_fuel_l1=math.fsum(errors)/len(errors)/amplitude,
        relative_energy_disturbance_l1=math.fsum(energy_errors)/len(energy_errors)/energy_scale,
        max_pressure_error_pa=max(abs(v-p) for v in pressure),duration_s=.001)
    return row

def diffusion(worker,n=16):
    c=config(n,diffusion=.1);u=[];a=.02;sinc=math.sin(math.pi/n)/(math.pi/n);p=101325.
    for q in range(n*16):
        change=a*sinc*math.sin(2*math.pi*(q%n+.5)/n);y=[.7-change,.2,0.,0.,.1+change]
        t=p/m.properties(y,300.)['R'];u+=m.conserved(y,1.,t)
    row=evolve(worker,c,u,1);out=row['final']['conserved'];num=[];den=[]
    decay=math.exp(-.1*(2*math.pi)**2*1e-6)
    for q in range(n*16):
        old=u[10*q+9];exact=.1+(old-.1)*decay
        num.append(abs(out[10*q+9]-exact));den.append(abs(exact-old))
    row.update(relative_diffusion_increment_l1=math.fsum(num)/math.fsum(den),
        comparison='One full conservative Euler step from uniform rho/p and zero velocity against continuum Fick sine decay; no sustained fixed-rho diffusion claim.')
    return row

def chemical_accounting(worker):
    # Prescribed 10% stoichiometric conversion in a uniform closed cell.
    # Formation is in E already. There is no extra heat-release source.
    y=m.mass_fractions([5.64,1.5,0,0,1]);u=m.conserved(y,1.,500.);source=[0.]*10
    mol_extent=.1*y[4]/m.MASS[4]
    source[6]=-1.5*mol_extent*m.MASS[1];source[7]=mol_extent*m.MASS[2]
    source[8]=2*mol_extent*m.MASS[3];source[9]=-mol_extent*m.MASS[4]
    c=config(4);row=evolve(worker,c,u*64,1,source*64)
    result=m.primitive(row['final']['conserved'][:10])
    row.update(initial_temperature_k=500.,final_temperature_k=result['temperature'],prescribed_fuel_conversion_fraction=.1,
        energy_source_j_m3=0.,scope='Stoichiometric formation-energy accounting only; not chemistry/kinetics/ignition.')
    return row

def contact(worker,velocity=0.):
    c=config(32);u=[];pressure=101325.
    for q in range(512):
        y=m.mass_fractions([3.76,1,0,0,.2] if q%32<16 else [5.64,0,1,2,0])
        t=500. if q%32<16 else 900.;rho=pressure/(m.properties(y,t)['R']*t)
        u+=m.conserved(y,rho,t,(velocity,0.,0.))
    row=evolve(worker,c,u,1000);out=row['final']['conserved']
    row.update(max_pressure_error_pa=max(abs(m.primitive(out[q:q+10])['pressure']-pressure) for q in range(0,len(out),10)),
        max_state_relative_error=max(abs(b-a)/max(1,abs(a)) for a,b in zip(u,out)),velocity_m_s=velocity,
        scope='Sharp fuel/air versus products contact: stationary exactness or moving-contact pressure-error assessment.')
    return row
