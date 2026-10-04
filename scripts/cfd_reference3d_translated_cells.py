"""Verify actual translated cell correspondence; rounded keys only find candidates."""
import numpy as np
def compare_cells(left,right,tolerance=1e-12):
    left=np.asarray(left,dtype=float);right=np.asarray(right,dtype=float)
    if left.ndim!=3 or left.shape[1:]!=(4,3) or right.ndim!=3 or right.shape[1:]!=(4,3) or not np.all(np.isfinite(left)) or not np.all(np.isfinite(right)) or tolerance!=1e-12:raise ValueError('finite tetrahedral cell coordinates and declared tolerance required')
    def index(cells):
        result={}
        for cell in cells:
            key=tuple(sorted(tuple(x) for x in np.round(cell,10)))
            if key in result:raise ValueError('candidate-key collision; no correspondence certified')
            result[key]=np.array(sorted(tuple(x) for x in cell))
        return result
    a=index(left);b=index(right)
    if a.keys()!=b.keys():return dict(cells_match=False,left_cells=len(a),right_cells=len(b),maximum_coordinate_difference_m=None,reason='actual cell correspondence differs')
    difference=max((float(np.max(np.abs(a[k]-b[k]))) for k in a),default=0.)
    return dict(cells_match=difference<=tolerance,left_cells=len(a),right_cells=len(b),maximum_coordinate_difference_m=difference,tolerance_m=tolerance,reason=None if difference<=tolerance else 'actual coordinate difference exceeds tolerance')
def inner_cells(saved,prefix):
    lo=saved[prefix+'lo'];hi=saved[prefix+'hi'];p=saved[prefix+'vertices_m'][:,saved[prefix+'tetrahedra']]
    mask=np.all((p[0]>=lo[0]-.1875-1e-12)&(p[0]<=hi[0]+.1875+1e-12),axis=0)
    return (p[:,:,mask]-np.array([lo[0],0.,0.])[:,None,None]).transpose(2,1,0)
