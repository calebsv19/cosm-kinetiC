"""Exact degree-three pressure integrals on clipped boxes; diagnostic trace only."""
import numpy as np
from skfem.quadrature import get_quadrature_tet
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_cubic_clip import clip_parts
ALPHA=np.array([(i,j,k) for i in range(4) for j in range(4-i) for k in range(4-i-j)])
NODES=ElementTetP3().doflocs.T
VANDERMONDE=np.array([np.prod(NODES**a[:,None],axis=0) for a in ALPHA]).T
INVERSE=np.linalg.inv(VANDERMONDE)
QUAD,WEIGHTS=get_quadrature_tet(3)
MONOMIALS=np.array([np.prod(QUAD**a[:,None],axis=0) for a in ALPHA])
TRACE_WEIGHTS=np.array([25.,-23.,13.,-3.])/12

class CubicPressureProjector:
    def __init__(self,mesh,pb,pressure):
        pressure=np.asarray(pressure)
        if pressure.shape!=(pb.N,) or not np.all(np.isfinite(pressure)) or pb.dofs.element_dofs.shape!=(20,mesh.nelements):raise ValueError('complete finite DG-P3 pressure required')
        self.mesh=mesh;self.corners=mesh.p[:,mesh.t].transpose(2,1,0)
        self.minimum=self.corners.min(axis=1);self.maximum=self.corners.max(axis=1)
        self.J=(self.corners[:,1:]-self.corners[:,:1]).transpose(0,2,1)
        self.det=np.abs(np.linalg.det(self.J));self.inverse=np.linalg.inv(self.J)
        self.coefficients=INVERSE@pressure[pb.dofs.element_dofs]
    def evaluate(self,cell,physical):
        X=self.inverse[cell]@(physical-self.corners[cell,0,:,None])
        return self.coefficients[:,cell]@np.array([np.prod(X**a[:,None],axis=0) for a in ALPHA])
    def integrate_box(self,lower,upper):
        lower=np.asarray(lower,dtype=float);upper=np.asarray(upper,dtype=float)
        if lower.shape!=(3,) or upper.shape!=(3,) or not np.all(np.isfinite(np.r_[lower,upper])) or np.any(upper<=lower):raise ValueError('invalid physical box')
        intersects=np.all((self.maximum>lower+1e-12)&(self.minimum<upper-1e-12),axis=1)
        inside=np.all((self.minimum>=lower-1e-12)&(self.maximum<=upper+1e-12),axis=1)
        ids=np.flatnonzero(inside);integral=0.;volume=float(self.det[ids].sum()/6)
        for begin in range(0,len(ids),512):
            cells=ids[begin:begin+512];integral+=float(np.sum(self.det[cells]*(self.coefficients[:,cells].T@MONOMIALS@WEIGHTS)))
        clipped=0;part_count=0
        for cell in np.flatnonzero(intersects & ~inside):
            parts=clip_parts(self.corners[cell],lower,upper)
            if not len(parts):continue
            J=(parts[:,1:]-parts[:,:1]).transpose(0,2,1);det=np.abs(np.linalg.det(J))
            points=parts[:,0,:,None]+J@QUAD
            values=self.evaluate(cell,points.transpose(1,0,2).reshape(3,-1)).reshape(len(parts),len(WEIGHTS))
            integral+=float(np.sum(det*(values@WEIGHTS)));volume+=float(det.sum()/6);clipped+=1;part_count+=len(parts)
        return dict(volume_m3=volume,pressure_integral_pa_m3=integral,clipped_tetrahedra=clipped,clipped_parts=part_count)

def interval_trace(averages):
    a=np.asarray(averages,dtype=float)
    if a.shape!=(4,) or not np.all(np.isfinite(a)):raise ValueError('four finite interval averages required')
    return float(TRACE_WEIGHTS@a)

def project_body_pressure(projector,lo,hi,n):
    if type(n) is not int or n not in (16,32):raise ValueError('unsupported declared Cartesian resolution')
    h=2./n;traces=[];slabs=[]
    for side in (0,1):
        averages=[]
        for depth in range(4):
            lower=lo.copy();upper=hi.copy()
            if side==0:lower[0]=lo[0]-(depth+1)*h;upper[0]=lo[0]-depth*h
            else:lower[0]=hi[0]+depth*h;upper[0]=hi[0]+(depth+1)*h
            row=projector.integrate_box(lower,upper);expected=float(np.prod(upper-lower))
            if abs(row['volume_m3']/expected-1)>1e-8:raise ValueError('reference does not cover complete fluid slab')
            average=row['pressure_integral_pa_m3']/expected;averages.append(average);slabs.append(dict(side=side,depth=depth,pressure_average_pa=average,**row))
        traces.append(interval_trace(averages))
    area=float(np.prod(hi[1:]-lo[1:]));return dict(n=n,front_trace_pa=traces[0],back_trace_pa=traces[1],native_trace_on_reference_n=area*(traces[0]-traces[1]),slabs=slabs,scope='degree-three-exact quadrature of the existing DG-P3 field on clipped Cartesian slabs followed by the experimental four-interval cubic-exact pressure stencil; not a replacement for raw pressure force or a complete field-error norm')
