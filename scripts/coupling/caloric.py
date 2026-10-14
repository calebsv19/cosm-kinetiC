"""Frozen-mixture sensible caloric reference, 200..1500 K, h(298.15)=0.
N2 NIST Shomate; O2/CO2/H2O GRI-Mech NASA7 Cp. No chemical formation
energy or reaction. Mole order N2, O2, CO2, H2O. Water vapor is a prescribed
ideal-gas species even where an equilibrium model would condense it.
"""
import math
from . import gas_mixture as nist
R=nist.R_UNIVERSAL
MASS=[.0280134,.0319988,.0440095,.0180153]
NASA=[
[[3.78245636,-2.99673416e-3,9.84730201e-6,-9.68129509e-9,3.24372837e-12],
 [3.28253784,1.48308754e-3,-7.57966669e-7,2.09470555e-10,-2.16717794e-14]],
[[2.35677352,8.98459677e-3,-7.12356269e-6,2.45919022e-9,-1.43699548e-13],
 [3.85746029,4.41437026e-3,-2.21481404e-6,5.23490188e-10,-4.72084164e-14]],
[[4.19864056,-2.0364341e-3,6.52040211e-6,-5.48797062e-9,1.77197817e-12],
 [3.03399249,2.17691804e-3,-1.64072518e-7,-9.7041987e-11,1.68200992e-14]]]
AIR=[3.76,1.,0.,0.]
PRODUCTS=[5.64,0.,1.,2.]
SOURCES=[nist.SOURCES['N2'],'https://raw.githubusercontent.com/Cantera/cantera/main/data/gri30.yaml']


def mixture(moles):
    if type(moles) is not list or len(moles)!=4 or any(type(v) not in (int,float) or not math.isfinite(v) or v<0 for v in moles):
        raise ValueError('four finite nonnegative supported mole amounts')
    total=math.fsum(moles);mass=math.fsum(v*m for v,m in zip(moles,MASS))
    if not math.isfinite(total) or not math.isfinite(mass) or total<=0 or mass<=0:raise ValueError('bounded positive mixture')
    return [v/mass for v in moles],R*total/mass


def temperature(t):
    if type(t) not in (int,float) or not math.isfinite(t) or not 200<=t<=1500:raise ValueError('published caloric domain 200..1500 K')
    return t


def cp_raw(index,t,upper):
    if index==0:
        a,b,c,d,e=nist.DATA['N2'][1][upper][2];x=t/1000
        return a+b*x+c*x*x+d*x*x*x+e/(x*x)
    return R*math.fsum(a*t**k for k,a in enumerate(NASA[index-1][upper]))


def join_correction(index):
    edge=500 if index==0 else 1000
    return cp_raw(index,edge,False)-cp_raw(index,edge,True)


def cp_species(index,t):
    upper=t >= (500 if index==0 else 1000)
    return cp_raw(index,t,upper)+(join_correction(index) if upper else 0.)


def enthalpy_species(index,t):
    if index==0:return nist.delta_h_molar('N2',298.15,t)+max(0,t-500)*join_correction(index)
    sign=-1 if t<298.15 else 1;low,high=sorted((t,298.15));terms=[]
    for lo,hi,coeff in ((200,1000,NASA[index-1][0]),(1000,1500,NASA[index-1][1])):
        left,right=max(low,lo),min(high,hi)
        if right>left:terms.append(R*math.fsum(a*(right**(k+1)-left**(k+1))/(k+1) for k,a in enumerate(coeff)))
    return sign*math.fsum(terms)+max(0,t-1000)*join_correction(index)


def properties(moles,t):
    temperature(t);weights,r=mixture(moles)
    cp=math.fsum(w*cp_species(i,t) for i,w in enumerate(weights));h=math.fsum(w*enthalpy_species(i,t) for i,w in enumerate(weights));cv=cp-r
    if cv<=0:raise ValueError('positive Cv')
    return dict(energy_j_kg=h-r*t,enthalpy_j_kg=h,cp_j_kg_k=cp,cv_j_kg_k=cv,
                gas_constant_j_kg_k=r,gamma=cp/cv,sound_speed_m_s=math.sqrt(cp/cv*r*t))


def invert(moles,energy):
    if not math.isfinite(energy):raise ValueError('finite energy')
    lo,hi=200.,1500.;el=properties(moles,lo)['energy_j_kg'];eh=properties(moles,hi)['energy_j_kg']
    tol=32*2.220446049250313e-16*max(1,abs(el),abs(eh))
    if energy<el-tol or energy>eh+tol:raise ValueError('caloric energy outside supported band')
    if energy<=el:return lo
    if energy>=eh:return hi
    t=lo+(hi-lo)*(energy-el)/(eh-el)
    for _ in range(40):
        p=properties(moles,t);error=p['energy_j_kg']-energy
        if abs(error)<=tol:return t
        if error<0:lo=t
        else:hi=t
        next_t=t-error/p['cv_j_kg_k']
        t=next_t if lo<next_t<hi else (lo+hi)/2
    raise ValueError('caloric inversion did not converge')


def gas_config(gas,moles):
    p=properties(moles,298.15)
    return dict(gas,caloric_moles=list(moles),gamma=p['gamma'],gas_constant_j_kg_k=p['gas_constant_j_kg_k'])


def conserved(moles,rho,t,velocity=(0.,0.,0.)):
    return [rho,*[rho*v for v in velocity],rho*properties(moles,t)['energy_j_kg']+.5*rho*math.fsum(v*v for v in velocity)]
