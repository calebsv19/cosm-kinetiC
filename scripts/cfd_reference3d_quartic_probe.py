#!/usr/bin/env python3
"""Higher-order Alfeld P4/DG-P3 reference; unchanged continuous Stokes and raw traction."""
import argparse
import json
import time
import resource
from pathlib import Path
import numpy as np
from scipy.sparse import hstack
from scipy.sparse.linalg import LinearOperator, minres
from skfem import FacetBasis, LinearForm, asm
from cfd_reference3d_spatial_mesh import spatial_mesh
from cfd_reference3d_quartic_pair import assemble_quartic,CubicPressureMass
from cfd_reference3d_quartic_observation import quartic_consistency
from cfd_reference3d_preconditioner import velocity_preconditioner,residual_diagnostics,array_sha
from cfd_reference3d_chunked import volume_metrics,reaction_weights,MixedOperator,saddle_digest
from cfd_reference3d_traction import traction


def run(kind='factor',maxiter=3000,length=4.,count=2,split=False,empty=False,target=1e-10,snapshot=None,normal_spacing=None,insert_normal=False,chunk_size=512):
    assert 1<=maxiter<=3000 and target in (1e-10,1e-12)
    started=time.monotonic();mu=.1;Q=.008
    mesh,lo,hi,axes,macro_count=spatial_mesh(length,not empty,count,split,n=4,normal_spacing=normal_spacing,insert_normal=insert_normal)
    ub,pb,A,blocks=assemble_quartic(mesh,mu,chunk_size)
    print(json.dumps({'phase':'forms','peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    fixed=ub.get_dofs(['walls']+(['body'] if not empty else [])).all()
    free=np.setdiff1d(np.arange(ub.N),fixed);Af=A[free][:,free]
    mass=CubicPressureMass(pb,mu)
    weights=reaction_weights(A,blocks,ub.get_dofs('body').all()) if not empty else None
    B=hstack([b[:,free] for b in blocks],format='csr')
    del A,blocks
    digest=saddle_digest(Af,B)
    K=MixedOperator.build(Af,B)
    print(json.dumps({'phase':'mixed-matrix','peak_rss_bytes':resource.getrusage(resource.RUSAGE_SELF).ru_maxrss}),flush=True)
    @LinearForm
    def inlet(v,w):return v
    f=asm(inlet,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['inlet'],intorder=8))
    nv=3*len(free);rhs=np.r_[f[free],np.zeros(2*len(free)+pb.N)]
    identity={'mesh_sha256':array_sha(mesh.p,mesh.t),'free_dofs_sha256':array_sha(free),
        'matrix_sha256':digest,'rhs_sha256':array_sha(rhs)}
    assembly=time.monotonic()-started
    mapping=mesh.mapping().A.transpose(2,0,1);condition=np.linalg.cond(mapping)
    diagnostics={'Jacobian_condition_median':float(np.median(condition)),'Jacobian_condition_max':float(condition.max())}
    print(json.dumps({'phase':'assembled','tetrahedra':mesh.nelements,'matrix_nnz':K.nnz,
        'velocity_free_dofs':nv,'pressure_dofs':int(pb.N),'identity':identity,'diagnostics':diagnostics}),flush=True)
    del B
    begin=time.monotonic();velocity,pc_meta=velocity_preconditioner(Af,kind)
    del Af
    def precondition(x):
        return np.r_[np.concatenate([velocity(v) for v in x[:nv].reshape(3,-1)]),mass.precondition(x[nv:])]
    setup=time.monotonic()-begin
    print(json.dumps({'phase':'solve','preconditioner':pc_meta,'assembly_s':assembly,'setup_s':setup}),flush=True)
    iterations=[0];progress=[]
    class Converged(Exception):
        def __init__(self,x):self.x=x.copy()
    def callback(x):
        iterations[0]+=1
        if iterations[0]%10==0:
            row=residual_diagnostics(K,x,rhs,nv,mass)
            if iterations[0]%100==0:
                row['iteration']=iterations[0];progress.append(row);print(json.dumps(row),flush=True)
            if row['true_residual']<=target:raise Converged(x)
    begin=time.monotonic()
    try:
        z,info=minres(K,rhs,M=LinearOperator(K.shape,precondition),rtol=1e-14,maxiter=maxiter,callback=callback)
    except Converged as accepted:z,info=accepted.x,0
    residual=residual_diagnostics(K,z,rhs,nv,mass);solve_s=time.monotonic()-begin
    accepted=info==0 and residual['true_residual']<1e-8
    row={'schema':'physics_sim_c3d_quartic_probe_v1','velocity_degree':4,'pressure_degree':3,'verified_volume_product_degree':6,'preconditioner':pc_meta,'pressure_preconditioner':{'kind':'exact_DG_cubic_mass'},
        'numerically_accepted':accepted,'physical_accuracy_certified':False,'info':int(info),
        'iterations':iterations[0],'iteration_limit':maxiter,'target':target,'final_residual':residual,
        'identity':identity,'diagnostics':diagnostics,'progress':progress,
        'timings':{'assembly_s':assembly,'setup_s':setup,'solve_s':solve_s},
        'matrix_free':True,'chunk_size':chunk_size,'insert_normal':insert_normal,'normal_spacing_m':normal_spacing,'length':length,'count':count,'split_first_normal':split,'body':not empty,
        'tetrahedra':mesh.nelements,'velocity_dofs':int(3*ub.N),'pressure_dofs':int(pb.N)}
    if not accepted:
        row['wall_s']=time.monotonic()-started
        return row
    u=np.zeros((3,ub.N));u[:,free]=z[:nv].reshape(3,-1);p=z[nv:]
    def facet(name):return FacetBasis(mesh,ub.elem,facets=mesh.boundaries[name],intorder=8)
    def values(b):return np.stack([b.interpolate(v) for v in u])
    def gradients(b):return np.stack([b.interpolate(v).grad for v in u])
    fi,fo=facet('inlet'),facet('outlet')
    def flux(b):return float(np.sum(np.einsum('i...,i...->...',values(b),b.normals)*b.dx))
    response=flux(fo);assert np.isfinite(response) and response>0
    Pin=Q/response;u*=Pin;p*=Pin
    assert np.all(np.isfinite(u)) and np.all(np.isfinite(p))
    D,div_l2,div_max=volume_metrics(mesh,ub,u,mu,chunk_size)
    power=Pin*Q+sum(float(np.sum(mu*np.einsum('i...,ji...,j...->...',values(b),gradients(b),b.normals)*b.dx)) for b in (fi,fo))
    checks=[traction(mesh,ub,pb,u,p,mu,lo,hi,order) for order in (4,8)] if not empty else []
    for key in ('pressure_force_n','raw_viscous_force_n','normal_viscous_force_n'):
        if checks:assert np.max(np.abs(np.array(checks[0][key])-checks[1][key]))<1e-10
    reaction=[float(-(weights[0]@u[a]+weights[1][a]@p-(f[ub.get_dofs('body').all()].sum()*Pin if a==0 else 0))) for a in range(3)] if not empty else [0.]*3
    row.update(inlet_pressure_pa=Pin,flow_m3_s=Q,mu=mu,axis_nodes_m=[a.tolist() for a in axes],
        pressure_force_n=checks[0]['pressure_force_n'] if checks else [0.]*3,
        raw_symmetric_viscous_force_n=checks[0]['raw_viscous_force_n'] if checks else [0.]*3,
        normal_viscous_force_n=checks[0]['normal_viscous_force_n'] if checks else [0.]*3,
        reaction_force_n=reaction,traction_checks=checks,
        consistency_diagnostics=quartic_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size) if not empty else None,
        physical_dissipation_w=D,physical_boundary_power_w=power,physical_energy_imbalance=abs(power-D)/D,
        divergence_l2_s_inv_m_3_2=div_l2,
        volume_divergence_max_s_inv=div_max,flux_error=abs(flux(fi)+Q)/Q)
    if snapshot is not None:
        assert not snapshot.exists()
        np.savez_compressed(snapshot,vertices_m=mesh.p,tetrahedra=mesh.t,velocity_coefficients=u,
            pressure_coefficients=p,velocity_doflocs_m=ub.doflocs,pressure_doflocs_m=pb.doflocs,
            lo=lo,hi=hi,mu=mu,length=length,velocity_degree=4,pressure_degree=3,numerically_accepted=True)
    row['peak_rss_bytes']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if row['peak_rss_bytes']>1800*1024**2:raise ValueError('own peak RSS cap exceeded')
    row['wall_s']=time.monotonic()-started
    return row


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--kind',choices=('amg','factor'),default='factor')
    ap.add_argument('--maxiter',type=int,default=3000);ap.add_argument('--length',type=float,choices=(4.,8.),default=4.)
    ap.add_argument('--count',type=int,choices=(2,4,6),default=2);ap.add_argument('--split',action='store_true')
    ap.add_argument('--empty',action='store_true');ap.add_argument('--target',type=float,choices=(1e-10,1e-12),default=1e-10)
    ap.add_argument('--normal-spacing',type=float);ap.add_argument('--insert-normal',action='store_true')
    ap.add_argument('--chunk-size',type=int,choices=(128,256,512,1024),default=512)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists()
    row=run(a.kind,a.maxiter,a.length,a.count,a.split,a.empty,a.target,a.snapshot,a.normal_spacing,a.insert_normal,a.chunk_size)
    a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps({'numerically_accepted':row['numerically_accepted'],'iterations':row['iterations'],'final_residual':row['final_residual'],'tetrahedra':row['tetrahedra']}),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
