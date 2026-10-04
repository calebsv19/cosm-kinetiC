#!/usr/bin/env python3
"""Independent P2/P1 Stokes reference; verification dependency, not runtime code.

Method/library reference: scikit-fem 12.0.2 examples 18 and 24:
https://scikit-fem.readthedocs.io/en/latest/listofexamples.html
Natural outlet is zero vector-Laplacian traction (mu*du/dn-p*n),
not an assertion of separately imposed zero pressure and velocity derivative.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import scipy
import skfem
from scipy.sparse import bmat
from skfem import MeshTri, ElementTriP2, ElementTriP1, ElementVector, Basis, FacetBasis, Functional, asm, condense, solve
from skfem.models.poisson import vector_laplace
from skfem.models.general import divergence


def reference(n, obstacle=True, length=4., mean=.002, corner_levels=0, sample_root=None):
    height=2.;mu=.1;width=.5
    mesh=MeshTri.init_tensor(np.linspace(0,length,round(length*n/height)+1),np.linspace(0,height,n+1))
    if obstacle:
        center=mesh.p[:,mesh.t].mean(axis=1)
        mesh=mesh.remove_elements(np.nonzero((center[0]>1.5)&(center[0]<2.5)&(center[1]>.75)&(center[1]<1.25))[0])
    # Refine geometrically around the four re-entrant fluid corners. The
    # integrable stress singularity slows separate surface-component convergence.
    if obstacle:
        corners=np.array([[1.5,.75],[1.5,1.25],[2.5,.75],[2.5,1.25]])
        for level in range(corner_levels):
            center=mesh.p[:,mesh.t].mean(axis=1).T
            distance=np.min(np.linalg.norm(center[:,None,:]-corners[None,:,:],axis=2),axis=1)
            mesh=mesh.refined(np.nonzero(distance<4*height/n/(2**level))[0])
    mesh=mesh.with_boundaries({
        'inlet':lambda x:np.isclose(x[0],0),
        'outlet':lambda x:np.isclose(x[0],length),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],height),
        'body':lambda x:((np.isclose(x[0],1.5)|np.isclose(x[0],2.5))&(x[1]>=.75)&(x[1]<=1.25))|((np.isclose(x[1],.75)|np.isclose(x[1],1.25))&(x[0]>=1.5)&(x[0]<=2.5))})
    ub=Basis(mesh,ElementVector(ElementTriP2()),intorder=4)
    pb=Basis(mesh,ElementTriP1(),intorder=4)
    a=mu*asm(vector_laplace,ub); b=-asm(divergence,ub,pb)
    matrix=bmat([[a,b.T],[b,None]],format='csr')
    prescribed=np.zeros(ub.N+pb.N)
    inlet=ub.get_dofs('inlet').all(['u^1']);y=ub.doflocs[1,inlet]/height
    prescribed[inlet]=6*mean*y*(1-y)
    fixed=ub.get_dofs(['inlet','walls']+(['body'] if obstacle else [])).all()
    result=solve(*condense(matrix,np.zeros(matrix.shape[0]),x=prescribed,D=fixed))
    velocity=result[:ub.N];pressure=result[ub.N:]
    residual=matrix@result
    free=np.setdiff1d(np.arange(len(result)),fixed)
    row={'n':n,'length':length,'height':height,'width':width,'mean_inlet_m_s':mean,'mu_pa_s':mu,
         'corner_refinement_levels':corner_levels,'model':'steady_Stokes_P2_P1','obstacle':obstacle,'velocity_dofs':int(ub.N),'pressure_dofs':int(pb.N),
         'free_equation_residual_inf':float(np.max(np.abs(residual[free]))),
         'outlet_condition':'zero vector-Laplacian traction; BC equivalence to MAC not assumed'}
    @Functional
    def flux(w):return w.u[0]*w.n[0]
    for name in ('inlet','outlet'):
        fb=FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=4)
        row[name+'_outward_flux_m3_s']=float(asm(flux,fb,u=fb.interpolate(velocity))*width)
    if obstacle:
        body_x=ub.get_dofs('body').all(['u^1'])
        row['reaction_drag_n']=float(-residual[body_x].sum()*width)
        uf=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['body'],intorder=4)
        pf=FacetBasis(mesh,pb.elem,facets=mesh.boundaries['body'],intorder=4)
        @Functional
        def pressure_force(w):return w.p*w.n[0]
        @Functional
        def shear_force(w):return -mu*(2*w.u.grad[0,0]*w.n[0]+(w.u.grad[0,1]+w.u.grad[1,0])*w.n[1])
        row['pressure_drag_n']=float(asm(pressure_force,uf,p=pf.interpolate(pressure))*width)
        row['viscous_drag_n']=float(asm(shear_force,uf,u=uf.interpolate(velocity))*width)
    else:
        # All scalar-component nodes, not only boundary nodes.
        indices=ub.split_indices()[0]
        y=ub.doflocs[1,indices]/height
        row['velocity_reference_error']=float(np.max(np.abs(velocity[indices]-6*mean*y*(1-y))))
        expected=12*mu*mean/(height*height)*(length-pb.doflocs[0])
        row['pressure_reference_error']=float(np.max(np.abs(pressure-expected)))
    if sample_root is not None and obstacle:
        sample_root.mkdir(parents=True,exist_ok=True)
        ui=ub.interpolator(velocity);pi=pb.interpolator(pressure)
        def masked(x,y):return (x>=1.5)&(x<=2.5)&(y>=.75)&(y<=1.25)
        def values(interpolator,points,components):
            result=np.zeros((components,points.shape[1]))
            active=np.nonzero(~masked(points[0],points[1]))[0]
            # Bounded batches avoid the element-finder's large broadcast allocation.
            for start in range(0,len(active),256):
                ids=active[start:start+256];result[:,ids]=interpolator(points[:,ids]).reshape(components,-1)
            return result
        for size in (16,32,64,128):
            x,y=np.meshgrid(np.linspace(0,length,size+1),(np.arange(size)+.5)*height/size)
            us=values(ui,np.vstack([x.ravel(),y.ravel()]),2)[0]
            x,y=np.meshgrid((np.arange(size)+.5)*length/size,np.linspace(0,height,size+1))
            vs=values(ui,np.vstack([x.ravel(),y.ravel()]),2)[1]
            x,y=np.meshgrid((np.arange(size)+.5)*length/size,(np.arange(size)+.5)*height/size)
            ps=values(pi,np.vstack([x.ravel(),y.ravel()]),1)[0]
            payload={'schema':'physics_sim_reference_field_samples_v1','n':size,'reference':row,'u':us.tolist(),'v':vs.tolist(),'p':ps.tolist()}
            (sample_root/f'fields-{size}.json').write_text(json.dumps(payload)+'\n')
    assert row['free_equation_residual_inf']<1e-9
    assert abs(row['inlet_outward_flux_m3_s']+row['outlet_outward_flux_m3_s'])<1e-9
    return row

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--grids',nargs='+',type=int,default=[16,32,64])
    p.add_argument('--length',type=float,default=4.)
    p.add_argument('--corner-levels',type=int,default=0)
    p.add_argument('--sample-root',type=Path)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    assert all(n>=8 and n%8==0 for n in args.grids)
    rows=[]
    calibration=reference(8,False,args.length)
    assert calibration['velocity_reference_error']<1e-10 and calibration['pressure_reference_error']<1e-9
    print(json.dumps(calibration),flush=True)
    for n in args.grids:
        row=reference(n,True,args.length,corner_levels=args.corner_levels,sample_root=args.sample_root);rows.append(row);print(json.dumps(row),flush=True)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps({'schema':'physics_sim_independent_stokes_reference_v1','versions':{'skfem':skfem.__version__,'numpy':np.__version__,'scipy':scipy.__version__},'calibration':calibration,'rows':rows,'mac_comparison_qualified':False},indent=2)+'\n')
