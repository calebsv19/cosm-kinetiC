"""Source-only conservative mixture adapter. Periodic nonreacting 3D, 200..1500 K.
Formation energy is included; prescribed equal-D diffusion, not calibrated transport.
"""
import hashlib,json,math,subprocess
from functools import lru_cache
from pathlib import Path
from . import caloric,hotgas
from surface_sources.growth_fire_v1 import digest,sealed
MASS=[.0280134,.0319988,.0440095,.01801528,.03204186]
R=caloric.R
FUEL=[[5.71539582,-.0152309129,6.52441155e-5,-7.10806889e-8,2.61352698e-11,-2.56427656e4],
[1.78970791,.0140938292,-6.36500835e-6,1.38171085e-9,-1.1706022e-13,-2.53748747e4]]
FORMATION=[0.,0.,R*(sum(a*298.15**(k+1)/(k+1) for k,a in enumerate(caloric.NASA[1][0]))-4.83719697e4)/MASS[2],
R*(sum(a*298.15**(k+1)/(k+1) for k,a in enumerate(caloric.NASA[2][0]))-3.02937267e4)/MASS[3],
R*(sum(a*298.15**(k+1)/(k+1) for k,a in enumerate(FUEL[0][:5]))+FUEL[0][5])/MASS[4]]
ELEMENTS=[[0,0,0,2],[0,0,2,0],[1,0,2,0],[0,2,1,0],[1,4,1,0]] # C,H,O,N


def mass_fractions(moles):
    if len(moles)!=5 or any(not math.isfinite(v) or v<0 for v in moles):raise ValueError('five supported amounts')
    weight=math.fsum(v*m for v,m in zip(moles,MASS))
    if not math.isfinite(weight) or weight<=0:raise ValueError('positive bounded mixture')
    return [v*m/weight for v,m in zip(moles,MASS)]


@lru_cache(maxsize=2048)
def pure(i,t):
    caloric.temperature(t)
    if i<4:
        moles=[0.]*4;moles[i]=1.;p=caloric.properties(moles,t);scale=caloric.MASS[i]/MASS[i]
        e=p['energy_j_kg']*scale;cp=p['cp_j_kg_k']*scale
    else:
        correction=R*(sum(a*1000**k for k,a in enumerate(FUEL[0][:5]))-sum(a*1000**k for k,a in enumerate(FUEL[1][:5])))
        coeff=FUEL[t>=1000];cp=(R*sum(a*t**k for k,a in enumerate(coeff[:5]))+(correction if t>=1000 else 0))/MASS[i]
        lo,hi=sorted((t,298.15));integral=0.
        for a,b,c in ((200,1000,FUEL[0]),(1000,1500,FUEL[1])):
            left,right=max(lo,a),min(hi,b)
            if right>left:integral+=R*sum(v*(right**(k+1)-left**(k+1))/(k+1) for k,v in enumerate(c[:5]))
        h=((integral if t>=298.15 else -integral)+max(0,t-1000)*correction)/MASS[i]
        e=h-R/MASS[i]*t
    return e+FORMATION[i],cp,R/MASS[i]


def properties(y,t):
    if len(y)!=5 or any(not math.isfinite(v) or v<0 for v in y) or abs(math.fsum(y)-1)>1e-11:raise ValueError('supported normalized mass fractions')
    p=[pure(i,t) for i in range(5)]
    e,cp,r=[math.fsum(v*row[k] for v,row in zip(y,p)) for k in range(3)]
    return dict(energy=e,cp=cp,R=r,cv=cp-r,enthalpy=e+r*t,sound=math.sqrt(cp/(cp-r)*r*t))


def conserved(y,rho,t,velocity=(0.,0.,0.)):
    p=properties(y,t)
    return [rho,*[rho*v for v in velocity],rho*p['energy']+.5*rho*math.fsum(v*v for v in velocity),*[rho*v for v in y]]


def primitive(u):
    if len(u)!=10 or any(not math.isfinite(v) for v in u) or u[0]<=0 or any(v<0 for v in u[5:]) or abs(math.fsum(u[5:])-u[0])>1e-11*u[0]:raise ValueError('positive density and matching species')
    y=[v/u[0] for v in u[5:]];e=u[4]/u[0]-.5*math.fsum((v/u[0])**2 for v in u[1:4]);lo,hi=200.,1500.
    el=properties(y,lo)['energy'];eh=properties(y,hi)['energy'];tol=1e-8
    if e<el-tol or e>eh+tol:raise ValueError('supported mixture energy')
    if e<=el:return dict(temperature=lo,pressure=u[0]*properties(y,lo)['R']*lo,y=y)
    if e>=eh:return dict(temperature=hi,pressure=u[0]*properties(y,hi)['R']*hi,y=y)
    t=lo+(hi-lo)*(e-el)/(eh-el)
    for _ in range(40):
        p=properties(y,t);error=p['energy']-e
        if abs(error)<=tol:return dict(temperature=t,pressure=u[0]*p['R']*t,y=y)
        if error<0:lo=t
        else:hi=t
        new=t-error/p['cv'];t=new if lo<new<hi else .5*(lo+hi)
    raise ValueError('mixture inversion')


def validate(config):
    if set(config)!={'grid','length_m','diffusion_m2_s','conductivity_w_m_k'}:raise ValueError('mixture configuration fields')
    n=config['grid'];length=config['length_m']
    if len(n)!=3 or any(type(v) is not int or not 4<=v<=256 for v in n) or math.prod(n)>65536:raise ValueError('mixture grid cap')
    if len(length)!=3 or any(not math.isfinite(v) or not .001<=v<=1000 for v in length):raise ValueError('bounded length')
    for k in ('diffusion_m2_s','conductivity_w_m_k'):
        if not math.isfinite(config[k]) or config[k]<0:raise ValueError('nonnegative transport')
    return 10*math.prod(n)


def identity(config,worker,source,rate):
    count=validate(config);hotgas.array(source,count);r=hotgas.rate(rate)
    recipes=[Path(__file__),Path(caloric.__file__),Path(caloric.nist.__file__),Path(hotgas.__file__)]
    return dict(model='periodic_five_species_formation_hllc_equal_D_fourier_v1',config_digest=digest(config),
        worker_sha256=hashlib.sha256(Path(worker).read_bytes()).hexdigest(),
        adapter_sha256=digest({str(p.name):hashlib.sha256(p.read_bytes()).hexdigest() for p in recipes}),
        source_digest=digest({'integrated_per_substep':source}),tick_seconds=[r.numerator,r.denominator])


def check_values(config,values):
    count=validate(config);hotgas.array(values,count)
    for q in range(0,count,10):primitive(values[q:q+10])


def initial(config,worker,values,source,rate):
    check_values(config,values)
    return sealed(dict(schema='mixture_checkpoint/v1',identity=identity(config,worker,source,rate),tick=0,conserved=list(values)))


def totals(config,values):
    volume=math.prod(config['length_m'])/math.prod(config['grid'])
    return [volume*math.fsum(values[i::10]) for i in range(10)]


def element_totals(partials):
    return [math.fsum(v/MASS[i]*ELEMENTS[i][a] for i,v in enumerate(partials)) for a in range(4)]


def advance(config,worker,checkpoint,source,end,rate):
    row=hotgas.checked(checkpoint);expected_identity=identity(config,worker,source,rate)
    if set(row)!={'schema','identity','tick','conserved'} or row['schema']!='mixture_checkpoint/v1' or row['identity']!=expected_identity:raise ValueError('mixture checkpoint identity drift')
    if type(row['tick']) is not int or type(end) is not int or not 0<=row['tick']<end<=2**53 or end-row['tick']>100000:raise ValueError('bounded exact ticks')
    check_values(config,row['conserved']);steps=end-row['tick']
    args=[*config['grid'],*config['length_m'],float(hotgas.rate(rate)),steps,config['diffusion_m2_s'],config['conductivity_w_m_k']]
    proc=subprocess.run([str(Path(worker).resolve()),*[format(v,'.17g') for v in args]],input=' '.join(format(v,'.17g') for v in row['conserved']+source),text=True,capture_output=True,timeout=60)
    if proc.returncode:raise ValueError('native mixture worker refused '+str(proc.returncode))
    result=json.loads(proc.stdout)
    if set(result)!={'conserved'}:raise ValueError('mixture output fields')
    values=result['conserved'];check_values(config,values)
    before,after,added=[totals(config,v) for v in (row['conserved'],values,source)]
    residual=[b-a-steps*s for a,b,s in zip(before,after,added)]
    for i,v in enumerate(residual):
        if abs(v)>1e-10*max(1,abs(before[i]),abs(after[i]),abs(steps*added[i])):raise ValueError('independent mixture conservation')
    state=sealed(dict(row,tick=end,conserved=values))
    receipt=sealed(dict(schema='mixture_step/v1',identity=expected_identity,prior_digest=checkpoint['digest'],next_digest=state['digest'],
        begin_tick=row['tick'],end_tick=end,totals_before=before,totals_after=after,integrated_source=[steps*s for s in added],
        conservation_residual=residual,elements_before=element_totals(before[5:]),elements_after=element_totals(after[5:]),
        source_elements=element_totals([steps*s for s in added[5:]])))
    return state,receipt
