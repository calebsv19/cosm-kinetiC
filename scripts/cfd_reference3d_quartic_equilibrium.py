"""Independent elementwise stress-equilibrium defects on approximate P4/DG-P3 fields.

F_raw - F_weak = J_interior - V_div_stress, with fluid outward normals. Pressure
and viscous weak pieces depend on the lift; none replaces raw physical traction.
"""
import numpy as np
from skfem import InteriorFacetBasis
from cfd_reference3d_p4 import ElementTetP4
from cfd_reference3d_chunked import cell_basis
from cfd_reference3d_mesh import edge_distance
from cfd_reference3d_traction import traction
ALPHA=np.array([(i,j,k) for i in range(5) for j in range(5-i) for k in range(5-i-j)])
NODES=ElementTetP4.doflocs.T
VANDERMONDE=np.array([np.prod(NODES**a[:,None],axis=0) for a in ALPHA]).T
INVERSE=np.linalg.inv(VANDERMONDE)
BANDS=np.array([.025,.05,.1,.2,.4,np.inf])


def monomial_hessians(X):
    result=np.zeros((3,3,len(ALPHA),X.shape[-1]))
    for m,a in enumerate(ALPHA):
        for i in range(3):
            for j in range(3):
                power=a.copy();factor=int(power[i]);power[i]-=1
                factor*=int(power[j]);power[j]-=1
                if factor>0 and np.all(power>=0):
                    result[i,j,m]=factor*np.prod(X**power[:,None],axis=0)
    return result


def velocity_hessian(ub,u,cells,X):
    local=u[:,ub.dofs.element_dofs[:,cells]]
    coefficients=np.einsum('mn,anc->amc',INVERSE,local)
    reference=np.einsum('amc,ijmq->aijcq',coefficients,monomial_hessians(X))
    inverse=ub.mesh.mapping().invA[:,:,cells]
    return np.einsum('ikc,jlc,aijcq->aklcq',inverse,inverse,reference,optimize=True)


def lifts(ub,lo,hi):
    distance=np.linalg.norm(np.maximum(np.maximum(lo[:,None]-ub.doflocs,ub.doflocs-hi[:,None]),0),axis=0)
    result=[]
    for shell in (.25,.4):
        t=np.minimum(distance/shell,1);eta=1-3*t*t+2*t*t*t
        assert np.max(np.abs(eta[ub.get_dofs('body').all()]-1))<1e-12
        assert np.max(np.abs(eta[ub.get_dofs(['walls','inlet','outlet']).all()]))<1e-12
        result.append((shell,eta))
    return result


def bucket_sums(values,coordinates,lo,hi):
    indices=np.searchsorted(BANDS,edge_distance(coordinates,lo,hi),side='right')
    return np.stack([np.bincount(indices,weights=v,minlength=len(BANDS)) for v in values],axis=1)


def diameters(vertices):
    return np.max([np.linalg.norm(vertices[:,i]-vertices[:,j],axis=0)
                   for i in range(vertices.shape[1]) for j in range(i)],axis=0)


def diagnose_equilibrium(mesh,ub,pb,u,p,mu,lo,hi,chunk_size=512,facet_order=8):
    assert 1<=chunk_size<=2048
    assert isinstance(ub.elem,ElementTetP4) and facet_order in (8,10)
    eta=lifts(ub,lo,hi);rows=[]
    for shell,_ in eta:
        row={'shell_m':shell,'pressure':{},'viscous':{}}
        for part in ('pressure','viscous'):
            row[part]={key:np.zeros(3) for key in ('weak_load_n','weighted_volume_divergence_n','interior_jump_n')}
            row[part]['volume_divergence_centroid_buckets_n']=np.zeros((len(BANDS),3))
            row[part]['interior_jump_centroid_buckets_n']=np.zeros((len(BANDS),3))
        rows.append(row)
    volume_residual_squared=0.;jump_squared=0.
    volume_buckets=np.zeros(len(BANDS));jump_buckets=np.zeros(len(BANDS))
    indicators=np.zeros(mesh.nelements)
    for begin in range(0,mesh.nelements,chunk_size):
        cells=np.arange(begin,min(begin+chunk_size,mesh.nelements));b=cell_basis(ub,cells);q=cell_basis(pb,cells)
        g=np.stack([b.interpolate(v).grad for v in u]);pv=q.interpolate(p);gradp=pv.grad
        H=velocity_hessian(ub,u,cells,b.X)
        viscous_div=mu*(np.einsum('akk...->a...',H)+np.einsum('bba...->a...',H))
        pressure_div=-gradp
        squared=np.sum(np.einsum('a...,a...->...',viscous_div+pressure_div,viscous_div+pressure_div)*b.dx,axis=1)
        volume_residual_squared+=float(squared.sum())
        vertices=mesh.p[:,mesh.t[:,cells]];center=vertices.mean(axis=1)
        weighted=diameters(vertices)**2*squared;indicators[cells]+=weighted
        volume_buckets+=bucket_sums(weighted[None],center,lo,hi)[:,0]
        for (_,lift),row in zip(eta,rows):
            field=b.interpolate(lift);grad=field.grad
            weak_pressure=pv*grad
            weak_viscous=-mu*np.einsum('aj...,j...->a...',g+g.swapaxes(0,1),grad)
            for part,weak,div in (('pressure',weak_pressure,pressure_div),('viscous',weak_viscous,viscous_div)):
                row[part]['weak_load_n']+=np.sum(weak*b.dx,axis=(1,2))
                weighted=np.sum(div*field*b.dx,axis=2)
                row[part]['weighted_volume_divergence_n']+=weighted.sum(axis=1)
                row[part]['volume_divergence_centroid_buckets_n']+=bucket_sums(weighted,center,lo,hi)
    interior=np.flatnonzero(mesh.f2t[1]!=-1)
    for begin in range(0,len(interior),chunk_size):
        faces=interior[begin:begin+chunk_size]
        left=InteriorFacetBasis(mesh,ub.elem,facets=faces,side=0,intorder=facet_order,dofs=ub.dofs)
        right=InteriorFacetBasis(mesh,ub.elem,facets=faces,side=1,quadrature=left.quadrature,dofs=ub.dofs)
        pl=InteriorFacetBasis(mesh,pb.elem,facets=faces,side=0,quadrature=left.quadrature,dofs=pb.dofs)
        pr=InteriorFacetBasis(mesh,pb.elem,facets=faces,side=1,quadrature=left.quadrature,dofs=pb.dofs)
        np.testing.assert_allclose(left.normals,right.normals,atol=1e-13)
        gl=np.stack([left.interpolate(v).grad for v in u]);gr=np.stack([right.interpolate(v).grad for v in u])
        pressure_jump=-(pl.interpolate(p)-pr.interpolate(p))*left.normals
        difference=gl-gr
        viscous_jump=mu*np.einsum('aj...,j...->a...',difference+difference.swapaxes(0,1),left.normals)
        jump=pressure_jump+viscous_jump
        squared=np.sum(np.einsum('a...,a...->...',jump,jump)*left.dx,axis=1)
        jump_squared+=float(squared.sum())
        vertices=mesh.p[:,mesh.facets[:,faces]];center=vertices.mean(axis=1)
        weighted=diameters(vertices)*squared
        np.add.at(indicators,mesh.f2t[0,faces],weighted/2)
        np.add.at(indicators,mesh.f2t[1,faces],weighted/2)
        jump_buckets+=bucket_sums(weighted[None],center,lo,hi)[:,0]
        for (_,lift),row in zip(eta,rows):
            field=left.interpolate(lift)
            for part,jump_part in (('pressure',pressure_jump),('viscous',viscous_jump)):
                weighted=np.sum(jump_part*field*left.dx,axis=2)
                row[part]['interior_jump_n']+=weighted.sum(axis=1)
                row[part]['interior_jump_centroid_buckets_n']+=bucket_sums(weighted,center,lo,hi)
    raw=traction(mesh,ub,pb,u,p,mu,lo,hi,facet_order)
    for row in rows:
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_viscous_force_n')):
            value=row[part];value['raw_surface_load_n']=np.array(raw[key])
            value['raw_minus_weak_n']=value['raw_surface_load_n']-value['weak_load_n']
            value['jump_minus_volume_n']=value['interior_jump_n']-value['weighted_volume_divergence_n']
            value['identity_error_n']=value['raw_minus_weak_n']-value['jump_minus_volume_n']
            for name,entry in value.items():value[name]=entry.tolist()
        row['total_identity_error_n']=(np.array(row['pressure']['identity_error_n'])+row['viscous']['identity_error_n']).tolist()
    return {'schema':'physics_sim_c3d_quartic_equilibrium_v1','physical_accuracy_certified':False,'lifts':rows,
        'volume_strong_equilibrium_defect_l2':volume_residual_squared**.5,
        'interior_stress_jump_l2':jump_squared**.5,
        'h_squared_volume_defect_centroid_buckets':volume_buckets.tolist(),
        'h_weighted_jump_defect_centroid_buckets':jump_buckets.tolist(),
        'equilibrium_indicator_squared_per_tet':indicators.tolist(),
        'equilibrium_indicator_scope':'h^2 volume and h interior-jump squared norms, split equally to adjacent cells; diagnostic refinement scores, not a proven force-error bound',
        'centroid_bucket_upper_distances_m':[float(v) if np.isfinite(v) else None for v in BANDS],
        'scope':'exact elementwise identity on approximate fields; centroid buckets are not clipped bands; no force replacement or physical certification'}
