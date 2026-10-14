"""Fixed physical-volume observations of native finite-volume gas energy.

Sampling volumes are independent of grid size. A Robin flux here is a
prescribed-boundary diagnostic; the existing insulated box exchanges zero heat.
"""
import math

RHO_CP=1.2*1005.


def mean_temperature(energy,n,lower,upper):
    if type(n) not in (int,list,tuple):raise ValueError('integer or rectangular grid required')
    grid=[n]*3 if type(n) is int else list(n)
    if len(grid)!=3 or any(type(v) is not int or v<=0 for v in grid) or len(energy)!=math.prod(grid) or len(lower)!=3 or len(upper)!=3 or not all(0<=a<b<=1 for a,b in zip(lower,upper)):
        raise ValueError('bounded three-dimensional sampling volume required')
    ranges=[range(max(0,math.floor(a*v)),min(v,math.ceil(b*v))) for a,b,v in zip(lower,upper,grid)]
    amounts=[]
    for k in ranges[2]:
        for j in ranges[1]:
            for i in ranges[0]:
                overlap=math.prod(max(0.,min((q+1)/v,b)-max(q/v,a)) for q,a,b,v in zip((i,j,k),lower,upper,grid))
                amounts.append(energy[(k*grid[1]+j)*grid[0]+i]*overlap*math.prod(grid))
    return 300.+math.fsum(amounts)/(RHO_CP*math.prod(b-a for a,b in zip(lower,upper)))


def observe(energy,n):
    # Common 32 vertical slabs over the exact authored source footprint.
    profile=[mean_temperature(energy,n,[.25,.25,k/32],[.75,.75,(k+1)/32])-300. for k in range(32)]
    faces={}
    # Gas volumes centered 1/16 m away from each face, thickness 1/32 m.
    # Edge/corner overlaps are avoided by the inset tangent footprint.
    for axis in range(3):
        for sign in (-1,1):
            lo=[.40625]*3;hi=[.59375]*3
            center=(.375 if sign<0 else .625)+sign*.0625
            lo[axis]=center-.015625;hi[axis]=center+.015625
            t=mean_temperature(energy,n,lo,hi)
            faces['xyz'[axis]+('-' if sign<0 else '+')]=dict(gas_temperature_k=t,
                sampling_lower_m=lo,sampling_upper_m=hi,
                diagnostic_robin_flux_w_m2=10.*(t-300.),actual_insulated_flux_w_m2=0.)
    return dict(source_footprint_temperature_rise_profile_k=profile,faces=faces)


def relative_l1(a,b):
    scale=math.fsum(abs(v) for v in b)
    if scale==0:
        return 0. if all(v==0 for v in a) else None
    return math.fsum(abs(x-y) for x,y in zip(a,b))/scale
