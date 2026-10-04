"""Fixed SPD repeated coupled two-block sweeps with exact principal factors."""
import numpy as np
from cfd_reference3d_triangle import SymmetricTriangle,validate_upper
from cfd_reference3d_shared_factor import validate_csr,storage_sha,SharedTriangleFactor


class RepeatedBlockCholesky:
    def __init__(self,velocity,library,cycles=1,ordering='metis',grouping='uv_w'):
        self.factors=[];self.closed=False
        if not isinstance(velocity,SymmetricTriangle):raise ValueError('explicit full symmetric-triangle action required')
        if not isinstance(cycles,int) or cycles not in (1,2,4,8):raise ValueError('invalid fixed sweep count')
        if grouping not in ('uv_w','u_vw'):raise ValueError('invalid component grouping')
        upper=velocity.upper;validate_csr(upper);validate_upper(upper)
        if upper.shape[0]%3:raise ValueError('three equal component blocks required')
        self.velocity=velocity;self.n=upper.shape[0]//3;self.cycles=cycles
        self.bounds=(0,2*self.n,3*self.n) if grouping=='uv_w' else (0,self.n,3*self.n)
        self.block_count=2
        self.upper={(a,b):upper[self.bounds[a]:self.bounds[a+1],self.bounds[b]:self.bounds[b+1]] for a in range(2) for b in range(a+1,2)}
        self.diagonal=[upper[self.bounds[a]:self.bounds[a+1],self.bounds[a]:self.bounds[a+1]] for a in range(2)]
        self.original_sha256=storage_sha(upper.indptr,upper.indices,upper.data)
        self.coupling_sha256=storage_sha(*(array for matrix in self.upper.values() for array in (matrix.indptr,matrix.indices,matrix.data)))
        try:
            for matrix in self.diagonal:self.factors.append(SharedTriangleFactor(matrix,library,ordering))
        except Exception:
            self.close();raise
        self.metadata=dict(kind='block_cholesky_ssor',grouping=grouping,block_dofs=[self.bounds[i+1]-self.bounds[i] for i in range(2)],fixed_sweep_count=cycles,component_count=3,component_dofs=self.n,
            block_factors=[factor.metadata for factor in self.factors],factor_storage_bytes=sum(factor.metadata['symbolic_factor_storage_bytes'] for factor in self.factors),
            factor_input_allocation_bytes=sum(factor.metadata['factor_input_allocation_bytes'] for factor in self.factors),
            diagonal_and_coupling_array_bytes=sum(m.indptr.nbytes+m.indices.nbytes+m.data.nbytes for m in (*self.diagonal,*self.upper.values())),
            scaling='none',input_preserved_after_factor=self.input_unchanged(),
            scope='fixed repeated SPD symmetric block sweep with exact positive principal factors and all component couplings; unchanged physical mixed operator')

    def sweep(self,x):
        forward=[]
        for a in range(2):
            rhs=x[self.bounds[a]:self.bounds[a+1]].copy()
            for b in range(a):rhs-=self.upper[b,a].T@forward[b]
            forward.append(self.factors[a].solve(rhs))
        result=[None]*2
        for a in reversed(range(2)):
            if a==1:result[a]=forward[a];continue
            correction=np.zeros(self.bounds[a+1]-self.bounds[a])
            for b in range(a+1,2):correction+=self.upper[a,b]@result[b]
            result[a]=forward[a]-self.factors[a].solve(correction)
        return np.concatenate(result)

    def solve(self,x):
        if self.closed:raise ValueError('component inverse is closed')
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=self.velocity.shape[:1] or not np.all(np.isfinite(rhs)):raise ValueError('invalid component inverse RHS')
        z=self.sweep(rhs)
        for _ in range(self.cycles-1):z+=self.sweep(rhs-self.velocity@z)
        if not np.all(np.isfinite(z)):raise ValueError('nonfinite repeated sweep action')
        return z

    def input_unchanged(self):
        upper=self.velocity.upper
        return self.original_sha256==storage_sha(upper.indptr,upper.indices,upper.data) and self.coupling_sha256==storage_sha(*(array for matrix in self.upper.values() for array in (matrix.indptr,matrix.indices,matrix.data))) and all(factor.input_unchanged() for factor in self.factors)

    def close(self):
        for factor in self.factors:factor.close()
        self.closed=True

    def __del__(self):self.close()
