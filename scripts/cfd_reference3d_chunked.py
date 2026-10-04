"""Bounded-cell assembly/observation of unchanged Alfeld P3/DG-P2 Stokes forms."""
import numpy as np
from scipy.sparse import csr_matrix
from skfem import Basis,FacetBasis,ElementDG,ElementTetP2,BilinearForm,asm
from skfem.models.poisson import laplace
from cfd_reference3d_p3 import ElementTetP3,require_sorted
from cfd_reference3d_preconditioner import PressureMass


def cell_basis(basis,indices):
    return Basis(basis.mesh,basis.elem,mapping=basis.mapping,quadrature=basis.quadrature,
                 elements=indices,dofs=basis.dofs)


def assemble_chunked(mesh,mu,chunk_size=1024):
    require_sorted(mesh);assert 1<=chunk_size<=4096
    ub=Basis(mesh,ElementTetP3(),intorder=4,elements=np.array([0]))
    pb=Basis(mesh,ElementDG(ElementTetP2()),quadrature=ub.quadrature,elements=np.array([0]))
    A=csr_matrix((ub.N,ub.N));blocks=[csr_matrix((pb.N,ub.N)) for _ in range(3)]
    for begin in range(0,mesh.nelements,chunk_size):
        indices=np.arange(begin,min(begin+chunk_size,mesh.nelements))
        u,p=cell_basis(ub,indices),cell_basis(pb,indices)
        A=A+mu*asm(laplace,u)
        for axis in range(3):
            @BilinearForm
            def divergence(v,q,w):return -v.grad[axis]*q
            blocks[axis]=blocks[axis]+asm(divergence,u,p)
    return ub,pb,A,blocks


def chunked_mass(pb,mu):
    mass=PressureMass(pb,mu)
    mass.determinant=np.abs(pb.mesh.mapping().detA)
    return mass


def volume_metrics(mesh,ub,u,mu,chunk_size):
    D=0.;div_squared=0.;div_max=0.
    for begin in range(0,mesh.nelements,chunk_size):
        b=cell_basis(ub,np.arange(begin,min(begin+chunk_size,mesh.nelements)))
        g=np.stack([b.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g)
        e=.5*(g+g.swapaxes(0,1))
        D+=float(np.sum(2*mu*np.einsum('ij...,ij...->...',e,e)*b.dx))
        div_squared+=float(np.sum(div**2*b.dx));div_max=max(div_max,float(np.max(np.abs(div))))
    return D,div_squared**.5,div_max


def diagnose_chunked(mesh,ub,pb,u,p,mu,lo,hi,chunk_size):
    fb=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['body'],intorder=4)
    g=np.stack([fb.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g)
    normal=fb.normals;normal_load=-2*mu*np.einsum('ii...->i...',g)*normal
    div_load=-2*mu*div*normal;x=fb.global_coordinates().mean(axis=2);faces=[]
    for axis in range(3):
        for side,plane in enumerate((lo[axis],hi[axis])):
            mask=np.isclose(x[axis],plane)[:,None]
            faces.append({'axis':axis,'side':side,
                'normal_load_n':np.sum(normal_load*fb.dx*mask,axis=(1,2)).tolist(),
                'divergence_load_n':np.sum(div_load*fb.dx*mask,axis=(1,2)).tolist(),
                'boundary_divergence_l2_s_inv_m':float(np.sum(div**2*fb.dx*mask))**.5})
    etas=[];lifts=[]
    distance=np.linalg.norm(np.maximum(np.maximum(lo[:,None]-ub.doflocs,ub.doflocs-hi[:,None]),0),axis=0)
    for shell in (.25,.4):
        t=np.minimum(distance/shell,1);eta=1-3*t*t+2*t*t*t
        assert np.max(np.abs(eta[ub.get_dofs('body').all()]-1))<1e-12
        assert np.max(np.abs(eta[ub.get_dofs(['walls','inlet','outlet']).all()]))<1e-12
        etas.append(eta);lifts.append({'shell_m':shell,'vector_laplacian_load_n':0.,
            'symmetric_stress_load_n':0.,'divergence_correction_n':0.})
    for begin in range(0,mesh.nelements,chunk_size):
        indices=np.arange(begin,min(begin+chunk_size,mesh.nelements))
        vb,vp=cell_basis(ub,indices),cell_basis(pb,indices)
        gradient=np.stack([vb.interpolate(v).grad for v in u]);pressure=vp.interpolate(p)
        divergence=np.einsum('ii...->...',gradient)
        for eta,lift in zip(etas,lifts):
            grad_eta=vb.interpolate(eta).grad
            vector=mu*np.einsum('j...,j...->...',gradient[0],grad_eta)-pressure*grad_eta[0]
            symmetric=mu*np.einsum('j...,j...->...',gradient[0]+gradient[:,0],grad_eta)-pressure*grad_eta[0]
            lift['vector_laplacian_load_n']-=float(np.sum(vector*vb.dx))
            lift['symmetric_stress_load_n']-=float(np.sum(symmetric*vb.dx))
            lift['divergence_correction_n']-=mu*float(np.sum(divergence*grad_eta[0]*vb.dx))
    for lift in lifts:
        lift['integration_by_parts_identity_error_n']=lift['symmetric_stress_load_n']-lift['vector_laplacian_load_n']-lift['divergence_correction_n']
    return {'normal_load_n':np.sum(normal_load*fb.dx,axis=(1,2)).tolist(),
        'normal_divergence_identity_error_n':np.sum((normal_load-div_load)*fb.dx,axis=(1,2)).tolist(),
        'faces':faces,'volume_lifts':lifts,
        'scope':'chunked independent diagnostics; raw surface traction remains the physical gate'}


def streamed_matrix_sha(matrix):
    """Canonical CSR digest without cloning the matrix or its data byte buffer."""
    import hashlib
    matrix.sort_indices();matrix.eliminate_zeros()
    h=hashlib.sha256()
    for a in (matrix.indptr,matrix.indices,matrix.data):
        x=np.ascontiguousarray(a);h.update(str((x.shape,x.dtype.str)).encode())
        view=memoryview(x).cast('B')
        for begin in range(0,len(view),1024**2):h.update(view[begin:begin+1024**2])
    return h.hexdigest()


def reaction_weights(A,blocks,body_dofs):
    velocity=np.asarray(A[body_dofs].sum(axis=0)).ravel()
    pressure=[np.asarray(b[:,body_dofs].sum(axis=1)).ravel() for b in blocks]
    return velocity,pressure


def mixed_matrix_lean(Af,B):
    """Build identical saddle rows through CSR stacks, avoiding full COO staging."""
    from scipy.sparse import kron,eye,hstack,vstack
    H=kron(eye(3),Af,format='csr')
    upper=hstack([H,B.T.tocsr()],format='csr')
    del H
    lower=hstack([B,csr_matrix((B.shape[0],B.shape[0]))],format='csr')
    return vstack([upper,lower],format='csr')


class MixedOperator:
    """Factory for the identical symmetric saddle operator without global copies."""
    @staticmethod
    def build(Af,B):
        from scipy.sparse.linalg import LinearOperator
        class Saddle(LinearOperator):
            def __init__(self):
                self.Af=Af;self.B=B;self.nf=Af.shape[0];self.nv=3*self.nf
                self.nnz=3*Af.nnz+2*B.nnz
                super().__init__(dtype=Af.dtype,shape=(self.nv+B.shape[0],)*2)
            def _matvec(self,x):
                velocity=np.concatenate([self.Af@v for v in x[:self.nv].reshape(3,-1)])
                velocity+=self.B.T@x[self.nv:]
                return np.r_[velocity,self.B@x[:self.nv]]
            def _rmatvec(self,x):return self._matvec(x)
        return Saddle()


def saddle_digest(Af,B):
    """Hash logical canonical CSR rows exactly as matrix_sha, without forming K."""
    import hashlib
    Af.sort_indices();Af.eliminate_zeros();B.sort_indices();B.eliminate_zeros()
    Bt=B.T.tocsr();nf=Af.shape[0];nv=3*nf
    counts=np.r_[np.tile(np.diff(Af.indptr),3)+np.diff(Bt.indptr),np.diff(B.indptr)]
    pointers=np.r_[np.array([0],dtype=B.indptr.dtype),np.cumsum(counts,dtype=B.indptr.dtype)]
    total=int(pointers[-1]);h=hashlib.sha256()
    def header(shape,dtype):h.update(str((shape,np.dtype(dtype).str)).encode())
    def push(a):
        view=memoryview(np.ascontiguousarray(a)).cast('B')
        for begin in range(0,len(view),1024**2):h.update(view[begin:begin+1024**2])
    header(pointers.shape,pointers.dtype);push(pointers)
    header((total,),B.indices.dtype)
    for r in range(nv):
        local=r%nf;offset=(r//nf)*nf
        push(Af.indices[Af.indptr[local]:Af.indptr[local+1]]+offset)
        push(Bt.indices[Bt.indptr[r]:Bt.indptr[r+1]]+nv)
    push(B.indices)
    header((total,),B.data.dtype)
    for r in range(nv):
        local=r%nf
        push(Af.data[Af.indptr[local]:Af.indptr[local+1]])
        push(Bt.data[Bt.indptr[r]:Bt.indptr[r+1]])
    push(B.data)
    return h.hexdigest()
