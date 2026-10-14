"""Linear right-going acoustic eigenmode of the frozen caloric Euler model."""
import math
from . import caloric,caloric_cases as cases,gas_mixture as nist,hotgas
from .hotgas_domain_cases import face


def case(worker,n):
    moles=caloric.PRODUCTS;c=cases.selected(moles,grid=(n,4,4),length=(1.,1.,1.));c['domain']['faces']=[face('periodic') for _ in range(6)]
    properties=caloric.properties(moles,900);r=properties['gas_constant_j_kg_k'];a=properties['sound_speed_m_s'];rho0=101325/(r*900);amplitude=.1;sinc=math.sin(math.pi/n)/(math.pi/n);u=[]
    for q in range(n*16):
        dp=amplitude*sinc*math.sin(2*math.pi*(q%n+.5)/n);rho=rho0+dp/a**2
        u+=caloric.conserved(moles,rho,(101325+dp)/(rho*r),(dp/(rho0*a),0.,0.))
    row=cases.evolve(worker,c,u,500,rate=(1,500000));_,p=hotgas.primitive(c['gas'],row['final']['conserved'])
    nist_sound=nist.properties(nist.METHANOL_PRODUCTS,900)['sound_speed_m_s']
    def error(sound):
        exact=[101325+amplitude*sinc*math.sin(2*math.pi*((q%n+.5)/n-sound*.001)) for q in range(n*16)]
        return math.fsum(abs(x-y) for x,y in zip(p,exact))/len(p)/amplitude
    row.update(analytic_sound_speed_m_s=a,nist_sound_speed_m_s=nist_sound,
               relative_pressure_error=error(a),nist_relative_pressure_error=error(nist_sound),duration_s=.001)
    return row
