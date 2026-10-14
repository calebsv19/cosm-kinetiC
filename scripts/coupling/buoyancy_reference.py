"""Independent linear acoustic/solenoidal buoyancy response, no fitted coefficients.

For v_t=-grad(pi)+b*ez, pi_t=-a² div(v), with fixed
b=B cos(kx*x) sin(kz*z), periodic x and impermeable z boundaries:
pi_eq=-B*kz/K² cos(kx*x) cos(kz*z), omega=a*K.
The gradient component oscillates acoustically; the divergence-free component
accelerates linearly. Hydrostatic compressible Euler approaches this short-time
reference as density contrast, g*L/(R*T), and displacement/L tend to zero.
It is not an exact solution for a finite-contrast 900 K bubble.
"""
import math
from . import hotgas, hotgas_domain as domain
from .hotgas_domain_cases import config,face,hydrostatic


def coefficients(length, temperature, gamma, gas_constant, buoyancy, time):
    kx=2*math.pi/length; kz=math.pi/length; k2=kx*kx+kz*kz
    sound=math.sqrt(gamma*gas_constant*temperature); omega=sound*math.sqrt(k2)
    acoustic=math.sin(omega*time)/omega
    return dict(horizontal=buoyancy*kx*kz/k2*(-time+acoustic),
        vertical=buoyancy*(kx*kx/k2*time+kz*kz/k2*acoustic),
        pressure_over_density=-buoyancy*kz/k2*(1-math.cos(omega*time)),
        omega_rad_s=omega,kx_m_inv=kx,kz_m_inv=kz)


def case(worker,flux,n,steps=4000,rate=(1,2000000),contrast=.001,prepared=False):
    c=config(grid=(n,4,n),length=(.1,.1,.1));c['domain']['flux']=flux
    c['domain']['faces']=[face('periodic') for _ in range(4)]+[face(),face()]
    initial_state=hydrostatic(c);ambient=list(initial_state)
    kx=2*math.pi/.1;kz=math.pi/.1;sx=math.sin(math.pi/n)/(math.pi/n)
    sz=math.sin(math.pi/(2*n))/(math.pi/(2*n));forms=[];buoyancy=9.81*contrast
    for q in range(n*4*n):
        x=(q%n+.5)*.1/n;z=(q//(n*4)+.5)*.1/n
        shapes=(math.sin(kx*x)*math.cos(kz*z)*sx*sz,
                math.cos(kx*x)*math.sin(kz*z)*sx*sz,
                math.cos(kx*x)*math.cos(kz*z)*sx*sz)
        forms.append(shapes);initial_state[5*q]*=1-contrast*shapes[1]
        if prepared:
            pi=-buoyancy*kz/(kx*kx+kz*kz)*shapes[2]
            initial_state[5*q+4]+=ambient[5*q]*pi/.4
    source=[0.]*len(initial_state);initial=domain.initial(c,worker,initial_state,source,rate)
    final,receipt=domain.advance(c,worker,initial,source,steps,rate)
    time=steps*float(hotgas.rate(rate));coef=coefficients(.1,300.,1.4,287.,buoyancy,time)
    if prepared:
        coef.update(horizontal=-buoyancy*kx*kz/(kx*kx+kz*kz)*time,
                    vertical=buoyancy*kx*kx/(kx*kx+kz*kz)*time,
                    pressure_over_density=-buoyancy*kz/(kx*kx+kz*kz))
    u=final['conserved'];_,pressures=hotgas.primitive(c['gas'],u);_,basep=hotgas.primitive(c['gas'],ambient)
    error=[];exact=[];pressure_error=[];pressure_exact=[]
    for q,(xshape,zshape,pshape) in enumerate(forms):
        vx=coef['horizontal']*xshape;vz=coef['vertical']*zshape
        error.append(abs(u[5*q+1]/u[5*q]-vx)+abs(u[5*q+3]/u[5*q]-vz));exact.append(abs(vx)+abs(vz))
        dp=ambient[5*q]*coef['pressure_over_density']*pshape
        pressure_error.append(abs(pressures[q]-basep[q]-dp));pressure_exact.append(abs(dp))
    return dict(config=c,initial=initial,final=final,receipt=receipt,rate=list(rate),duration_s=time,
        density_contrast=contrast,well_prepared=prepared,reference_coefficients=coef,
        relative_velocity_l1=math.fsum(error)/math.fsum(exact),
        relative_pressure_perturbation_l1=math.fsum(pressure_error)/max(1e-30,math.fsum(pressure_exact)),
        small_parameters=dict(density_contrast=contrast,stratification=9.81*.1/(287*300),
            displacement_over_length=buoyancy*time*time/.1,expected_mach=buoyancy*time/math.sqrt(1.4*287*300)),
        reference_scope='Linear frozen-buoyancy acoustic adjustment; finite-contrast and nonlinear/stratification truncation remain.')
