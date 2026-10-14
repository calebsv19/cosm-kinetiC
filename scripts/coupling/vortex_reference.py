"""Stationary Gresho vortex: independent exact radial Euler force balance.

Manufactured constant-gamma numerical fixture, not a physical burner/air state.
The stationary variant sets background translation to zero. Cell-average
conserved states are initialized by product Gauss quadrature; metrics compare
those averages, not point values. Pressure offsets vary Mach without changing
velocity, geometry or the specified convective duration.
"""
import math
from . import hotgas,hotgas_domain as domain
from .hotgas_domain_cases import config,face
# Four-point Gauss-Legendre rule on [-1,1], normalized cell-average weights.
NODES=(-.8611363115940526,-.3399810435848563,.3399810435848563,.8611363115940526)
WEIGHTS=(.3478548451374538,.6521451548625461,.6521451548625461,.3478548451374538)
SOURCE='https://arxiv.org/html/2405.11063v2#S3.SS1'


def profile(x,y,mach,gamma=1.4):
    x-=.5;y-=.5;r=math.hypot(x,y)
    if r<.2: speed=5*r;p=12.5*r*r
    elif r<.4: speed=2-5*r;p=4*math.log(5*r)+4-20*r+12.5*r*r
    else: speed=0.;p=4*math.log(2)-2
    vx=-speed*y/r if r else 0.;vy=speed*x/r if r else 0.
    pressure=1/(gamma*mach*mach)-.5+p
    return [1.,vx,vy,0.,pressure/(gamma-1)+.5*speed*speed]


def initial_state(n,mach):
    row=[]
    for y in range(n):
        for x in range(n):
            terms=[[] for _ in range(5)]
            for xi,wx in zip(NODES,WEIGHTS):
                for yi,wy in zip(NODES,WEIGHTS):
                    u=profile((x+.5+.5*xi)/n,(y+.5+.5*yi)/n,mach)
                    for i in range(5):terms[i].append(wx*wy*u[i]/4)
            row.extend(math.fsum(v) for v in terms)
    return row*4


def case(worker,flux,n=16,mach=.01,duration=(1,50),batch=400):
    c=config(grid=(n,n,4),length=(1.,1.,1.),gravity=(0.,0.,0.));c['domain']['flux']=flux
    c['domain']['faces']=[face('periodic') for _ in range(6)]
    u=initial_state(n,mach);source=[0.]*len(u)
    # Physical acoustic CFL <= .2; denominator multiple 50 gives exact .02 s.
    maximum=math.sqrt(1.4*(1/(1.4*mach*mach)+4*math.log(2)-2))+1
    denominator=50*math.ceil(maximum*(2*n+4)/(.2*50));rate=(1,denominator)
    seconds=hotgas.rate(duration);steps=seconds*denominator
    if steps.denominator!=1:raise ValueError('duration must align exactly')
    steps=int(steps);initial=domain.initial(c,worker,u,source,rate);current=initial;receipts=[]
    while current['tick']<steps:
        current,r=domain.advance(c,worker,current,source,min(steps,current['tick']+batch),rate);receipts.append(r)
    final=current;v=final['conserved'];_,p0=hotgas.primitive(c['gas'],u);_,p1=hotgas.primitive(c['gas'],v)
    mean0=math.fsum(p0)/len(p0);mean1=math.fsum(p1)/len(p1)
    velocity_error=math.fsum(abs(v[5*q+i]/v[5*q]-u[5*q+i]) for q in range(len(u)//5) for i in (1,2))
    velocity_scale=math.fsum(abs(u[5*q+i]) for q in range(len(u)//5) for i in (1,2))
    pressure_error=math.fsum(abs((b-mean1)-(a-mean0)) for a,b in zip(p0,p1))
    pressure_scale=math.fsum(abs(a-mean0) for a in p0)
    def kinetic(state):return math.fsum(math.fsum(state[5*q+i]**2 for i in (1,2,3))/(2*state[5*q]) for q in range(len(state)//5))
    return dict(config=c,initial=initial,final=final,receipts=receipts,rate=list(rate),duration_s=float(seconds),mach_parameter=mach,
        relative_velocity_l1=velocity_error/velocity_scale,relative_pressure_shape_l1=pressure_error/pressure_scale,
        kinetic_energy_ratio=kinetic(v)/kinetic(u),pressure_range_pa=[min(p1),max(p1)],
        normalized_pressure_range=(max(p1)-min(p1))/(mean1*mach*mach),
        max_mass_momentum_energy_residual=max(abs(x) for r in receipts for x in r['conservation_residual']),
        numerical_reference='Stationary Gresho radial force balance; four-point product cell-average quadrature; fixed velocity and exact specified convective duration.',
        source=SOURCE,physical_flame_qualified=False)
