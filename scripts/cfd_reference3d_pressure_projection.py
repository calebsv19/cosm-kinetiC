#!/usr/bin/env python3
"""Exact affine P1 pressure integration over near-body Cartesian slabs.

No point sampling: clip each tetrahedral polyhedron with the six slab planes,
triangulate its faces about an interior point, and integrate affine pressure by
centroids. A trace-functional decomposition is not a complete field-error norm.
"""
import argparse
import hashlib
import json
import time
from pathlib import Path
import numpy as np


def clip_tetra(vertices, lower, upper):
    faces=[vertices[list(ids)].copy() for ids in ((0,1,2),(0,1,3),(0,2,3),(1,2,3))]
    tolerance=1e-12
    for axis in range(3):
        for sign,plane in ((-1,lower[axis]),(1,upper[axis])):
            clipped=[];cuts=[]
            for polygon in faces:
                points=[];previous=polygon[-1];dprev=sign*(previous[axis]-plane)
                for current in polygon:
                    dnow=sign*(current[axis]-plane)
                    iprev=dprev<=tolerance;inow=dnow<=tolerance
                    if iprev!=inow:
                        point=previous+(current-previous)*dprev/(dprev-dnow)
                        point[axis]=plane;points.append(point);cuts.append(point)
                    if inow:points.append(current)
                    previous=current;dprev=dnow
                if len(points)>=3:clipped.append(np.array(points))
            if len(cuts)>=3:
                points=np.unique(np.round(cuts,12),axis=0)
                if len(points)>=3:
                    other=[a for a in range(3) if a!=axis];center=points.mean(axis=0)
                    angle=np.arctan2(points[:,other[1]]-center[other[1]],points[:,other[0]]-center[other[0]])
                    clipped.append(points[np.argsort(angle)])
            faces=clipped
            if not faces:return 0.,np.zeros(3)
    center=np.concatenate(faces).mean(axis=0)
    volume=0.;moment=np.zeros(3)
    for polygon in faces:
        for i in range(1,len(polygon)-1):
            triangle=polygon[[0,i,i+1]]
            v=abs(np.linalg.det(triangle-center))/6
            volume+=v;moment+=v*(center+triangle.sum(axis=0))/4
    return volume,moment


def integrate_box(vertices,tetrahedra,pressure,lower,upper):
    corners=vertices[tetrahedra]
    minimum,maximum=corners.min(axis=1),corners.max(axis=1)
    intersects=np.all((maximum>lower+1e-12)&(minimum<upper-1e-12),axis=1)
    inside=np.all((minimum>=lower-1e-12)&(maximum<=upper+1e-12),axis=1)
    matrix=corners[:,1:]-corners[:,:1]
    volumes=np.abs(np.linalg.det(matrix))/6
    volume=float(np.sum(volumes[inside]));integral=float(np.sum(volumes[inside]*pressure[tetrahedra[inside]].mean(axis=1)))
    count=0
    for cell in np.nonzero(intersects & ~inside)[0]:
        v,moment=clip_tetra(corners[cell],lower,upper)
        if v<=1e-24:continue
        q=pressure[tetrahedra[cell]]
        gradient=np.linalg.solve(matrix[cell],q[1:]-q[0])
        integral+=q[0]*v+gradient@(moment-corners[cell,0]*v)
        volume+=v;count+=1
    return {'volume_m3':volume,'pressure_integral_pa_m3':integral,'clipped_tetrahedra':count}


def project(path,n):
    started=time.monotonic();snapshot=np.load(path)
    vertices=snapshot['vertices_m'].T;tetrahedra=snapshot['tetrahedra'].T;pressure=snapshot['pressure_pa']
    lo,hi=snapshot['lo'],snapshot['hi'];h=2/n
    traces=[];slabs=[]
    for side in (0,1):
        total=0
        for depth,weight in enumerate((11/6,-7/6,2/6)):
            lower=lo.copy();upper=hi.copy()
            if side==0:lower[0]=lo[0]-(depth+1)*h;upper[0]=lo[0]-depth*h
            else:lower[0]=hi[0]+depth*h;upper[0]=hi[0]+(depth+1)*h
            box=integrate_box(vertices,tetrahedra,pressure,lower,upper)
            expected=float(np.prod(upper-lower))
            assert abs(box['volume_m3']/expected-1)<1e-8,box
            average=box['pressure_integral_pa_m3']/expected
            total+=weight*average
            slabs.append(dict(side=side,depth=depth,pressure_average_pa=average,**box))
        traces.append(total)
    return {'n':n,'native_trace_on_reference_n':traces[0]-traces[1],
            'front_trace_pa':traces[0],'back_trace_pa':traces[1],'slabs':slabs,
            'wall_s':time.monotonic()-started,'snapshot_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'scope':'exact affine P1 pressure volume projection followed by native interval-average pressure trace'}


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('snapshot',type=Path);ap.add_argument('--n',type=int,required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists();row=project(a.snapshot,a.n);a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row))
