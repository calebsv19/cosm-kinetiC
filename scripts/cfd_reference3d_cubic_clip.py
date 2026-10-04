"""Original convex tetra/box clipping partition; cubic quadrature is separate."""
import numpy as np

def clip_parts(vertices, lower, upper):
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
            if not faces:return np.empty((0,4,3))
    center=np.concatenate(faces).mean(axis=0)
    parts=[]
    for polygon in faces:
        for i in range(1,len(polygon)-1):
            triangle=polygon[[0,i,i+1]]
            if abs(np.linalg.det(triangle-center))>6e-24:parts.append(np.vstack((center,triangle)))
    return np.array(parts).reshape(-1,4,3)
