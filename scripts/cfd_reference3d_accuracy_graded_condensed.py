"""Exact P4/DG-P3 macro elimination; reconstruct original pressure fluctuations.

Local retained variables: 3*34 trace velocities and one constant pressure.
Eliminated variables: 3*35 velocity bubbles and 79 orthogonal pressure modes.
No physical stiffness, pressure penalty, smoothing or force replacement.
"""
import numpy as np
from scipy.linalg import block_diag,solve
from scipy.sparse import coo_matrix
from skfem import MeshTet,Basis,ElementDG,BilinearForm,asm
from cfd_reference3d_p3 import alfeld_split,ElementTetP3
from cfd_reference3d_p4 import ElementTetP4,require_sorted
from cfd_reference3d_quartic_pair import quartic_quadrature
from cfd_reference3d_chunked import cell_basis


class ReferenceMacro:
    def __init__(self):
        vertices=np.array([[0.,1.,0.,0.],[0.,0.,1.,0.],[0.,0.,0.,1.]])
        self.mesh=alfeld_split(MeshTet(vertices,np.arange(4)[:,None]))
        self.ub=Basis(self.mesh,ElementTetP4(),quadrature=quartic_quadrature())
        self.pb=Basis(self.mesh,ElementDG(ElementTetP3()),quadrature=self.ub.quadrature)
        assert self.ub.N==69 and self.pb.N==80
        self.trace=self.ub.get_dofs().all();self.bubble=np.setdiff1d(np.arange(69),self.trace)
        assert len(self.trace)==34 and len(self.bubble)==35
        v=np.eye(80)[0]-np.ones(80)/np.sqrt(80);v/=np.linalg.norm(v)
        H=np.eye(80)-2*np.outer(v,v)
        self.Q=H[:,1:];self.P=np.column_stack((np.ones(80),self.Q))
        np.testing.assert_allclose(self.Q.T@np.ones(80),0,atol=1e-14)
        self.ext=np.r_[np.concatenate([self.trace+a*69 for a in range(3)]),207]
        self.inner=np.r_[np.concatenate([self.bubble+a*69 for a in range(3)]),np.arange(208,287)]
        self.stiffness=np.empty((3,3,69,69));self.divergence=np.empty((3,80,69))
        for i in range(3):
            for j in range(3):
                @BilinearForm
                def cross(u,v,w):return u.grad[i]*v.grad[j]
                self.stiffness[i,j]=asm(cross,self.ub).toarray()
            @BilinearForm
            def coupling(u,q,w):return -u.grad[i]*q
            self.divergence[i]=asm(coupling,self.ub,self.pb).toarray()
    def forms(self,J,mu):
        inverse=np.linalg.inv(J);det=abs(np.linalg.det(J))
        metric=inverse@inverse.T
        A=mu*det*np.einsum('ij,ijab->ab',metric,self.stiffness)
        B=det*np.einsum('ia,ipn->apn',inverse,self.divergence)
        return A,B
    def matrix(self,J,mu):
        A,B=self.forms(J,mu);coupling=np.hstack(B)
        top=coupling.T@self.P
        return np.block([[block_diag(A,A,A),top],[top.T,np.zeros((80,80))]])
    def eliminated(self,J,mu):
        K=self.matrix(J,mu);D=K[np.ix_(self.inner,self.inner)];T=K[np.ix_(self.inner,self.ext)]
        R=-solve(D,T,assume_a='sym')
        error=float(np.linalg.norm(D@R+T)/max(np.linalg.norm(T),1e-30))
        assert error<1e-10,('local elimination residual',error)
        S=K[np.ix_(self.ext,self.ext)]+T.T@R
        asymmetry=float(np.linalg.norm(S-S.T)/max(np.linalg.norm(S),1e-30))
        assert asymmetry<1e-10,('local Schur symmetry',asymmetry)
        # Restore only floating-point symmetry of the exact symmetric Schur form.
        return (S+S.T)/2,R,error,asymmetry


class CondensedSystem:
    def __init__(self,mesh,mu,cache_cap_bytes=64*1024**2):
        require_sorted(mesh);assert 0<=cache_cap_bytes<=64*1024**2
        self.mesh=mesh;self.mu=mu;self.nmacro=mesh.nelements//4
        assert mesh.nelements==4*self.nmacro and mesh.nelements<=120000
        self.ref=ReferenceMacro()
        self.ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]))
        self.pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=self.ub.quadrature,elements=np.array([0]))
        self.records=[];inside=[];self.cache={};max_center_error=0.
        for macro in range(self.nmacro):
            cells=macro+self.nmacro*np.arange(4);vertices=np.unique(mesh.t[:,cells]);assert len(vertices)==5
            center=vertices[-1];corners=vertices[:-1];points=mesh.p[:,corners]
            J=points[:,1:]-points[:,0,None]
            max_center_error=max(max_center_error,float(np.max(np.abs(mesh.p[:,center]-points.mean(axis=1)))))
            mapping={int(v):i for i,v in enumerate(vertices)}
            local=np.array([[mapping[int(v)] for v in mesh.t[:,c]] for c in cells]).T
            order=[]
            for face in self.ref.mesh.t.T:
                found=np.flatnonzero(np.all(local==face[:,None],axis=0));assert len(found)==1;order.append(int(found[0]))
            ordered=cells[order]
            globalu=np.empty(69,dtype=np.int32)
            globalu[self.ref.ub.dofs.element_dofs.ravel()]=self.ub.dofs.element_dofs[:,ordered].ravel()
            assert np.array_equal(globalu[self.ref.ub.dofs.element_dofs],self.ub.dofs.element_dofs[:,ordered])
            globalp=self.pb.dofs.element_dofs[:,ordered].T.ravel()
            bubble=globalu[self.ref.bubble];inside.extend(bubble)
            key=J.tobytes()
            self.records.append((J,globalu,globalp,key))
        assert max_center_error<1e-12
        assert len(np.unique(inside))==35*self.nmacro
        self.trace=np.setdiff1d(np.arange(self.ub.N),inside);self.nt=len(self.trace)
        self.trace_inverse=np.full(self.ub.N,-1,dtype=np.int32);self.trace_inverse[self.trace]=np.arange(self.nt)
        self.shape=(3*self.nt+self.nmacro,)*2
        nentries=self.nmacro*103**2
        rr=np.empty(nentries,dtype=np.int32);cc=np.empty(nentries,dtype=np.int32);values=np.empty(nentries)
        errors=[];asymmetries=[];volumes=[];assembly_cache={};cache_bytes=0;cache_cap=cache_cap_bytes
        geometry_classes=len({row[3] for row in self.records})
        for macro,(J,globalu,globalp,key) in enumerate(self.records):
            if key in assembly_cache:
                S,error,asymmetry=assembly_cache[key]
            else:
                S,R,error,asymmetry=self.ref.eliminated(J,mu)
                if cache_bytes+R.nbytes<=cache_cap:
                    self.cache[key]=(R,);cache_bytes+=R.nbytes
                    assembly_cache[key]=(S,error,asymmetry)
            errors.append(error);asymmetries.append(asymmetry);volumes.append(abs(np.linalg.det(J))/6)
            trace=self.trace_inverse[globalu[self.ref.trace]];assert np.all(trace>=0)
            ids=np.r_[np.concatenate([trace+a*self.nt for a in range(3)]),3*self.nt+macro]
            offset=slice(macro*103**2,(macro+1)*103**2)
            rr[offset]=np.repeat(ids,103);cc[offset]=np.tile(ids,103);values[offset]=S.ravel()
        self.matrix=coo_matrix((values,(rr,cc)),shape=self.shape).tocsr()
        self.matrix.eliminate_zeros();self.volumes=np.array(volumes)
        self.metadata=dict(full_velocity_dofs=int(3*self.ub.N),full_pressure_dofs=int(self.pb.N),
            condensed_velocity_dofs=3*self.nt,condensed_pressure_dofs=self.nmacro,
            local_eliminated_unknowns=184,local_retained_unknowns=103,
            geometry_classes=geometry_classes,reconstruction_cache_classes=len(self.cache),reconstruction_cache_cap_bytes=cache_cap,reconstruction_cache_bytes=sum(row[0].nbytes for row in self.cache.values()),
            maximum_local_elimination_residual=max(errors),maximum_local_schur_asymmetry=max(asymmetries),
            maximum_alfeld_center_error_m=max_center_error,condensed_matrix_nnz=int(self.matrix.nnz),
            condensation_scope='exact local mixed elimination and full-field reconstruction; floating-point Schur symmetry restored, no physical penalty')
    def _inner_load(self,record,rhs):
        J,globalu,globalp,key=record
        uforce=rhs[:3*self.ub.N].reshape(3,-1)
        pforce=rhs[3*self.ub.N:]
        return np.r_[uforce[:,globalu[self.ref.bubble]].ravel(),self.ref.Q.T@pforce[globalp]]
    def _local_indices(self,macro,globalu):
        trace=self.trace_inverse[globalu[self.ref.trace]]
        return np.r_[np.concatenate([trace+a*self.nt for a in range(3)]),3*self.nt+macro]
    def _load_solution(self,record,load):
        if not np.any(load):return np.zeros(184)
        K=self.ref.matrix(record[0],self.mu)
        return solve(K[np.ix_(self.ref.inner,self.ref.inner)],load,assume_a='sym')
    def reduce_rhs(self,rhs):
        assert rhs.shape==(3*self.ub.N+self.pb.N,)
        force=rhs[:3*self.ub.N].reshape(3,-1);pressure=rhs[3*self.ub.N:]
        result=np.r_[force[:,self.trace].ravel(),np.array([pressure[p].sum() for _,_,p,_ in self.records])]
        for macro,record in enumerate(self.records):
            load=self._inner_load(record,rhs)
            if np.any(load):
                K=self.ref.matrix(record[0],self.mu);T=K[np.ix_(self.ref.inner,self.ref.ext)]
                result[self._local_indices(macro,record[1])]-=T.T@self._load_solution(record,load)
        return result
    def reconstruct(self,z,rhs):
        assert z.shape==(self.shape[0],)
        u=np.zeros((3,self.ub.N));p=np.zeros(self.pb.N);u[:,self.trace]=z[:3*self.nt].reshape(3,-1)
        for macro,record in enumerate(self.records):
            J,globalu,globalp,key=record
            outside=z[self._local_indices(macro,globalu)]
            R=self.cache[key][0] if key in self.cache else self.ref.eliminated(J,self.mu)[1]
            eliminated=R@outside+self._load_solution(record,self._inner_load(record,rhs))
            u[:,globalu[self.ref.bubble]]=eliminated[:105].reshape(3,35)
            p[globalp]=outside[-1]+self.ref.Q@eliminated[105:]
        return u,p
    def retained_field(self,u,p):
        return np.r_[u[:,self.trace].ravel(),np.array([p[indices].mean() for _,_,indices,_ in self.records])]


def full_action(mesh,ub,pb,u,p,mu,chunk_size=512):
    """Independent original FE quadrature action, without condensed/local matrices."""
    momentum=np.zeros((3,ub.N));continuity=np.zeros(pb.N)
    for begin in range(0,mesh.nelements,chunk_size):
        cells=np.arange(begin,min(begin+chunk_size,mesh.nelements));b=cell_basis(ub,cells);q=cell_basis(pb,cells)
        gradient=np.stack([b.interpolate(v).grad for v in u]);pressure=q.interpolate(p)
        derivative=np.array([v[0].grad for v in b.basis]);pressure_basis=np.array([v[0] for v in q.basis])
        local=mu*np.einsum('aicq,jicq,cq->ajc',gradient,derivative,b.dx)-np.einsum('cq,jacq,cq->ajc',pressure,derivative,b.dx)
        for a in range(3):np.add.at(momentum[a],ub.dofs.element_dofs[:,cells].ravel(),local[a].ravel())
        divergence=np.einsum('aacq->cq',gradient)
        continuity[q.dofs.element_dofs[:,cells].ravel()]=-np.einsum('jcq,cq,cq->jc',pressure_basis,divergence,b.dx).ravel()
    return np.r_[momentum.ravel(),continuity]
