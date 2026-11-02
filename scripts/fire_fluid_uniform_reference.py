"""Positive continuous cell-average kernel and Decimal spectral operator checks.

No native solver calls. The spatial spectrum is evaluated at two precisions
only at late samples, where its cancellation can be explicitly qualified.
"""
from decimal import Decimal as D, localcontext, getcontext
import math
import numpy as np

ALPHA = .026/1206
PI = D('3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647')


def remainder(distance, sigma):
    """Gaussian twice-integrated tail, with no subtraction of large linears."""
    z = abs(distance)/(math.sqrt(2)*sigma)
    if z < 20:
        factor = 1-math.sqrt(math.pi)*z*math.exp(z*z)*math.erfc(z)
    else:
        term = factor = 1/(2*z*z)
        for k in range(2, 18):
            term *= -(2*k-1)/(2*z*z)
            factor += term
    return sigma/math.sqrt(2*math.pi)*math.exp(-z*z)*factor


def continuous(time, velocity, lo, hi, points=128, images=1):
    if time == 0:return 0.
    nodes, weights = np.polynomial.legendre.leggauss(points)
    # age=s^2 removes the adjacent-support sqrt(age) endpoint singularity.
    a, b = math.sqrt(max(0,time-.4)), math.sqrt(time)
    roots = a+(nodes+1)*(b-a)/2
    result = 0.
    for root, weight in zip(roots, weights*(b-a)/2):
        age=root*root; sigma=math.sqrt(2*ALPHA*age);mean=0.
        for image in range(-images,images+1):
            sl=1/32+velocity*age+image*.25;sh=1/16+velocity*age+image*.25
            overlap=max(0,min(hi,sh)-max(lo,sl))
            value=(overlap+remainder(hi-sl,sigma)-remainder(hi-sh,sigma)
                   -remainder(lo-sl,sigma)+remainder(lo-sh,sigma))/(hi-lo)
            assert math.isfinite(value) and value>=0
            mean+=value
        result += weight*2*root*mean
    return float(result)


def sincos(x):
    x=x%(2*PI)
    if x>PI:x-=2*PI
    if x < -PI:x+=2*PI
    xx=-x*x;st=x;ct=D(1);s=st;c=ct;k=1
    epsilon=D(10)**(-(getcontext().prec+2))
    while abs(st)>epsilon or abs(ct)>epsilon:
        st*=xx/D((2*k)*(2*k+1));ct*=xx/D((2*k-1)*(2*k))
        s+=st;c+=ct;k+=1
    return s,c


def add(a,b):return a[0]+b[0],a[1]+b[1]
def mul(a,b):return a[0]*b[0]-a[1]*b[1],a[0]*b[1]+a[1]*b[0]
def div(a,b):
    den=b[0]*b[0]+b[1]*b[1]
    return (a[0]*b[0]+a[1]*b[1])/den,(a[1]*b[0]-a[0]*b[1])/den


def power(a,n):
    result=(D(1),D(0))
    while n:
        if n&1:result=mul(result,a)
        a=mul(a,a);n//=2
    return result


def exponential(a,time):
    if a[0]*time < -D(3)*(getcontext().prec+10):
        return D(0),D(0)
    scale=(a[0]*time).exp();s,c=sincos(a[1]*time)
    return scale*c,scale*s


def spectrum(velocity, precision=60, times=('.4','.475','.5')):
    """Exact semidiscrete and exact BE/source-pulse spectra on 4096 cells.

    Source cells512:1024, observation1024:1536 or1536:2048. Exact
    interval sums supply the DFT weights. The native matrix is never called.
    """
    with localcontext() as ctx:
        ctx.prec=precision
        alpha=D(26)/1206000;u=D(str(velocity));dx=D(1)/16384;dt=D(1)/25600
        rows=[dict(time_s=float(t),semi=[D(t)/8 if D(t)<D('.4') else D('.4')/8]*2,
                   be=[D(t)/8 if D(t)<D('.4') else D('.4')/8]*2,
                   continuous256=[D('.4')/8]*2,continuous512=[D('.4')/8]*2) for t in times]
        roots=[sincos(PI*m/4) for m in range(8)]
        for m in range(1,2048):
            if m%8==0:continue  # Exactly zero box-source coefficient.
            theta=2*PI*m/4096;s,c=sincos(theta);half_s,_=sincos(theta/2)
            box_s,_=sincos(PI*(m%16)/8)
            amplitude=box_s*box_s/(D(4096*512)*half_s*half_s)
            lam=(-2*alpha*(1-c)/(dx*dx)-u*(1-c)/dx,-u*s/dx)
            denominator=(-lam[0],-lam[1]);r=div((D(1),D(0)),(1-dt*lam[0],-dt*lam[1]))
            for row,t in zip(rows,times):
                end=D(t);duration=min(end,D('.4'));age=max(0,end-D('.4'))
                sem=div(add((D(1),D(0)),tuple(-v for v in exponential(lam,duration))),denominator)
                sem=mul(sem,exponential(lam,age))
                be=div(add((D(1),D(0)),tuple(-v for v in power(r,int(duration/dt)))),denominator)
                be=mul(be,power(r,int(age/dt)))
                for k in range(2):
                    sn,cs=roots[(m*(k+1))%8]
                    weight=(2*amplitude*cs,2*amplitude*sn)
                    row['semi'][k]+=mul(weight,sem)[0];row['be'][k]+=mul(weight,be)[0]
                if age>0 and m<=512:
                    wave=2*PI*m/D('.25');cl=(-alpha*wave*wave,-u*wave)
                    response=mul(div(add((D(1),D(0)),tuple(-v for v in exponential(cl,duration))),(-cl[0],-cl[1])),exponential(cl,age))
                    ca=D('.125')*(box_s/(PI*m/8))**2
                    for k in range(2):
                        sn,cs=roots[(m*(k+1))%8];delta=mul((2*ca*cs,2*ca*sn),response)[0]
                        row['continuous512'][k]+=delta
                        if m<=256:row['continuous256'][k]+=delta
        return [dict(time_s=r['time_s'],**{name:{k:str(v) for k,v in zip(['transport_k','tail_k'],r[name])}
                     for name in ['semi','be']+(['continuous256','continuous512'] if r['time_s']>.4 else [])}) for r in rows]
