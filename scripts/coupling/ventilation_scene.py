"""Cutaway display of the channel's immutable boundary geometry; not a solver."""
from .ventilation import CHANNEL


def cutaway_surfaces(config,plane):
    f=config['fluid'];w=f.get('wind_forcing',{})
    if w.get('schema')!=CHANNEL:return []
    x,y,z=f['length_m'];objects=[
        plane('channel_back',[x/2,y,z/2],x,z,(1,0,0),(0,0,1),(0,-1,0)),
        plane('channel_ceiling',[x/2,y/2,z],x,y,(1,0,0),(0,-1,0),(0,0,-1))]
    lo=w['inlet_patch_m']['lower_yz_m'];hi=w['inlet_patch_m']['upper_yz_m']
    spans=[(0,lo[0],0,z),(hi[0],y,0,z),(lo[0],hi[0],0,lo[1]),(lo[0],hi[0],hi[1],z)]
    side=w['inlet']=='x_max';u=(0,-1,0) if side else (0,1,0);normal=(-1,0,0) if side else (1,0,0)
    for i,(a,b,c,d) in enumerate(spans):
        if b>a and d>c:objects.append(plane('channel_inlet_frame_'+str(i),[x if side else 0,(a+b)/2,(c+d)/2],b-a,d-c,u,(0,0,1),normal))
    return objects
