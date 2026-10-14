"""Selected stability controls for the pressure-corrected compressible method."""
import math
from . import hotgas,hotgas_domain as domain
from .hotgas_domain_cases import config,face,conserved


def noise(worker,gravity=False,steps=2000):
    c=config(grid=(8,8,8),length=(.1,.1,.1),gravity=(0.,0.,-9.81) if gravity else (0.,0.,0.))
    c['domain']['flux']='lhllc_muscl'
    c['domain']['faces']=[face('periodic') for _ in range(4)]+[face(),face()] if gravity else [face('periodic') for _ in range(6)]
    if gravity:
        from .hotgas_domain_cases import hydrostatic
        u=hydrostatic(c)
    else:u=conserved(101325/(287*300),300.)*512
    ambient=list(u);seed=314159
    for q in range(512):
        seed=(1664525*seed+1013904223)%2**32;dp=.001*(seed/2**32-.5)
        u[5*q+4]+=dp/.4
    source=[0.]*len(u);rate=(1,250000);initial=domain.initial(c,worker,u,source,rate);current=initial;receipts=[]
    while current['tick']<steps:
        current,r=domain.advance(c,worker,current,source,min(steps,current['tick']+250),rate);receipts.append(r)
    _,pbase=hotgas.primitive(c['gas'],ambient)
    def acoustic_energy(state):
        _,p=hotgas.primitive(c['gas'],state)
        return math.fsum(.5*math.fsum(state[5*q+a]**2 for a in (1,2,3))/state[5*q]+
            (p[q]-pbase[q])**2/(2*state[5*q]*1.4*287*300) for q in range(512))
    return dict(config=c,initial=initial,final=current,receipts=receipts,rate=list(rate),duration_s=steps/250000,
        gravity=gravity,acoustic_energy_ratio=acoustic_energy(current['conserved'])/acoustic_energy(u),
        max_mass_momentum_energy_residual=max(abs(v) for r in receipts for v in r['conservation_residual']),
        interpretation='Deterministic broadband pressure noise at physical acoustic CFL about 0.333; bounded 8 ms stability probe, not an all-CFL or long-time guarantee.')
