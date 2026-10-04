#!/usr/bin/env python3
"""Independent continuous half-cell trilinear reconstruction; never reads matrix work."""
import argparse,json,struct,time
from pathlib import Path
import numpy as np


def reconstruct(path, center=2):
    data=Path(path).read_bytes();nx,ny,nz=struct.unpack_from('=3i',data)
    cells=nx*ny*nz;raw=np.array(struct.unpack_from('='+str(4*cells+ny*nz)+'d',data,12))
    lower=raw[:4*cells].reshape(nz,ny,nx,4);upper=raw[4*cells:].reshape(nz,ny)
    N=[nx,ny,nz];h=2/ny;nodes=[]
    q=[np.arange(2*n+1)*.5 for n in N]
    for a in range(3):
        shape=[nx,ny,nz];shape[a]+=1
        field=np.zeros(shape[::-1]);field[:nz,:ny,:nx]=lower[:,:,:,a]
        if a==0:field[:,:,-1]=upper
        positions=[q[b]-(0 if a==b else .5) for b in range(3)]
        positions=[np.clip(p,0,shape[b]-1) for b,p in enumerate(positions)]
        low=[np.floor(p).astype(int) for p in positions];high=[np.minimum(l+1,shape[b]-1) for b,l in enumerate(low)]
        fraction=[p-l for p,l in zip(positions,low)]
        out=np.zeros((2*nz+1,2*ny+1,2*nx+1))
        for i in (0,1):
            for j in (0,1):
                for k in (0,1):
                    ix=high[0] if i else low[0];iy=high[1] if j else low[1];iz=high[2] if k else low[2]
                    weight=(fraction[2] if k else 1-fraction[2])[:,None,None]*(fraction[1] if j else 1-fraction[1])[None,:,None]*(fraction[0] if i else 1-fraction[0])[None,None,:]
                    out+=field[np.ix_(iz,iy,ix)]*weight
        out[:,0,:]=out[:,-1,:]=0;out[0,:,:]=out[-1,:,:]=0
        lo=np.rint(np.array([center-.5,.5,.5])*2/h).astype(int);hi=np.rint(np.array([center+.5,1.5,1.5])*2/h).astype(int)
        out[lo[2]:hi[2]+1,lo[1]:hi[1]+1,lo[0]:hi[0]+1]=0
        nodes.append(out)
    return nodes,h,lo,hi


def energy(nodes,h,lo,hi):
    shape=tuple(n-1 for n in nodes[0].shape);fluid=np.ones(shape,dtype=bool)
    fluid[lo[2]:hi[2],lo[1]:hi[1],lo[0]:hi[0]]=False
    delta=h/2;D=0;div2=0
    t=[.5-.5/np.sqrt(3),.5+.5/np.sqrt(3)]
    for tx in t:
        for ty in t:
            for tz in t:
                location=[tx,ty,tz];grad=[]
                for a in range(3):
                    row=[]
                    for b in range(3):
                        out=np.zeros(shape)
                        for i in (0,1):
                            for j in (0,1):
                                for k in (0,1):
                                    bits=[i,j,k];weight=1/delta
                                    for c in range(3):weight*= (1 if bits[c] else -1) if b==c else (location[c] if bits[c] else 1-location[c])
                                    out+=weight*nodes[a][k:k+shape[0],j:j+shape[1],i:i+shape[2]]
                        row.append(out)
                    grad.append(row)
                density=np.zeros(shape)
                for a in range(3):
                    for b in range(3):density+=.2*(.5*(grad[a][b]+grad[b][a]))**2
                D+=float(density[fluid].sum())*delta**3/8
                div2+=float(((grad[0][0]+grad[1][1]+grad[2][2])**2)[fluid].sum())*delta**3/8
    return D,div2**.5


def trace(nodes,h,lo,hi,wall=False):
    forces=np.zeros(3);delta=h/2
    for a in range(3) if not wall else (1,2):
        for side in (0,1):
            index=(hi[a]+1 if side else lo[a]-1) if not wall else (nodes[0].shape[2-a]-2 if side else 1)
            slices=[slice(lo[c],hi[c]+1) if not wall else slice(None) for c in range(3)]
            slices[a]=index
            for b in range(3):
                if b==a:continue
                f=nodes[b][tuple(slices[::-1])]
                forces[b]+=.1/delta*((f[:-1,:-1]+f[1:,:-1]+f[:-1,1:]+f[1:,1:])/4).sum()*delta**2
    return forces.tolist()


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('binary',type=Path);ap.add_argument('--center',type=float,default=2)
    a=ap.parse_args();start=time.monotonic();nodes,h,lo,hi=reconstruct(a.binary,a.center)
    D,div=energy(nodes,h,lo,hi)
    print(json.dumps(dict(physical_dissipation_w=D,reconstruction_divergence_l2=div,body_viscous_force_n=trace(nodes,h,lo,hi),wall_force_n=trace(nodes,h,lo,hi,True),wall_s=time.monotonic()-start)))
