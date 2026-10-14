"""Matched numerical references for contact, acoustics and transient buoyancy."""
import math
from . import hotgas, hotgas_domain as domain
from .hotgas_domain_cases import config, face, conserved, hydrostatic


def evolve(worker, c, u, steps, rate=(1,1000000)):
    source=[0.]*len(u); initial=domain.initial(c,worker,u,source,rate)
    final,receipt=domain.advance(c,worker,initial,source,steps,rate)
    return initial,final,receipt


def stationary_contact(worker, flux):
    c=config(grid=(16,4,4),length=(1.,1.,1.),gravity=(0.,0.,0.))
    c['domain']['faces']=[face('periodic') for _ in range(6)];c['domain']['flux']=flux
    u=[]
    for q in range(256):
        rho=1. if q%16<8 else 1/3
        u+=conserved(rho,300/rho)
    initial,final,receipt=evolve(worker,c,u,1000)
    return dict(config=c,initial=initial,final=final,receipt=receipt,
        max_density_error_kg_m3=max(abs(a-b) for a,b in zip(u[::5],final['conserved'][::5])),
        interpretation='Exact zero-gravity stationary isobaric contact; inviscid Euler has no heat diffusion.')


def acoustic(worker, flux, nx, boundary='periodic', steps=1000):
    c=config(grid=(nx,4,4),length=(1.,1.,1.),gravity=(0.,0.,0.))
    c['domain']['flux']=flux;c['domain']['faces'][0]=face(boundary);c['domain']['faces'][1]=face(boundary)
    rho0=101325/(287*300);a=math.sqrt(1.4*287*300);amplitude=.1;u=[];dt=1e-6
    # Exact cell-average of the linear right-moving pressure eigenmode.
    sinc=math.sin(math.pi/nx)/(math.pi/nx)
    for q in range(nx*16):
        perturbation=amplitude*sinc*math.sin(2*math.pi*(q%nx+.5)/nx)
        rho=rho0+perturbation/(a*a);velocity=perturbation/(rho0*a)
        p=101325+perturbation
        u+=conserved(rho,p/(rho*287),(velocity,0.,0.))
    initial,final,receipt=evolve(worker,c,u,steps)
    pressures=hotgas.primitive(c['gas'],final['conserved'])[1]
    exact=[101325+amplitude*sinc*math.sin(2*math.pi*((q%nx+.5)/nx-a*steps*dt)) for q in range(nx*16)]
    return dict(config=c,initial=initial,final=final,receipt=receipt,
        amplitude_pa=amplitude,duration_s=steps*dt,analytic_phase_speed_m_s=a,
        pressure_l1_error_pa=math.fsum(abs(p-e) for p,e in zip(pressures,exact))/len(exact),
        relative_pressure_l1_error=math.fsum(abs(p-e) for p,e in zip(pressures,exact))/len(exact)/amplitude,
        interpretation='Linear acoustic reference; O(amplitude squared) nonlinear error. First-order acoustic diffusion remains.')


def pressure_pulse(worker, boundary, flux="hllc"):
    c=config(grid=(128,4,4),length=(1.,1.,1.),gravity=(0.,0.,0.));c['domain']['flux']=flux
    c['domain']['faces'][0]=face(boundary);c['domain']['faces'][1]=face(boundary)
    rho0=101325/(287*300);a=math.sqrt(1.4*287*300);u=[]
    for q in range(128*16):
        x=(q%128+.5)/128;dp=.1*math.exp(-((x-.75)/.04)**2)
        rho=rho0+dp/(a*a);p=101325+dp
        u+=conserved(rho,p/(rho*287),(dp/(rho0*a),0.,0.))
    initial,final,receipt=evolve(worker,c,u,1500)
    def characteristic(state):
        pressures=hotgas.primitive(c['gas'],state)[1];right=[];left=[]
        for q in range(128):
            dp=pressures[q]-101325;velocity=state[5*q+1]/state[5*q]
            right.append((dp+rho0*a*velocity)/2);left.append((dp-rho0*a*velocity)/2)
        return dict(right_l1_pa=math.fsum(abs(v) for v in right)/128,
                    left_l1_pa=math.fsum(abs(v) for v in left)/128,
                    reflected_pressure_sign_at_peak=left[max(range(128),key=lambda i:abs(left[i]))])
    before=characteristic(u);after=characteristic(final['conserved'])
    return dict(config=c,initial=initial,final=final,receipt=receipt,
        before=before,after=after,left_reflection_l1_ratio=after['left_l1_pa']/before['right_l1_pa'],
        interpretation='Specified reservoir ghost p and velocity; this small-amplitude measurement does not qualify nonlinear nonreflecting behavior.')


def bubble(worker, flux, scale=2, steps=1000, height=1., rate=(1,500000)):
    nx=6*scale;nz=round(16*scale*height); c=config(grid=(nx,nx,nz),length=(.5,.5,height))
    c['domain']['flux']=flux;c['domain']['faces']=[face('reservoir') for _ in range(6)];c['domain']['faces'][4]=face()
    u=hydrostatic(c);indices=[]
    # Same physical support [.1667,.3333]^2 x [.375,.625], aligned meshes for height 1 or 2.
    for q in range(nx*nx*nz):
        x=(q%nx+.5)*.5/nx;y=(q//nx%nx+.5)*.5/nx;z=(q//(nx*nx)+.5)*height/nz
        if 1/6<x<1/3 and 1/6<y<1/3 and .375<z<.625:
            u[5*q]/=3;indices.append(q)
    initial,final,receipt=evolve(worker,c,u,steps,rate)
    temperatures,_=hotgas.primitive(c['gas'],final['conserved'])
    mean=math.fsum(final['conserved'][5*q+3]/final['conserved'][5*q] for q in indices)/len(indices)
    weights=[max(t-300,0)*final['conserved'][5*q] for q,t in enumerate(temperatures)]
    weighted=math.fsum(w*final['conserved'][5*q+3]/final['conserved'][5*q] for q,w in enumerate(weights))/math.fsum(weights)
    return dict(config=c,initial=initial,final=final,receipt=receipt,duration_s=steps*float(hotgas.rate(rate)),rate=list(rate),
        selected_region_mean_w_m_s=mean,thermal_excess_weighted_w_m_s=weighted,
        temperature_range_k=[min(temperatures),max(temperatures)],
        physical_buoyant_accuracy_qualified=False)
