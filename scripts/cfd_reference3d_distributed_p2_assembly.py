"""Bounded exact local Galerkin assembly; original mixed assembly is unchanged."""
import json,resource,time
import numpy as np
from scipy.sparse import coo_matrix
from cfd_reference3d_quadratic_coarse import macro_quadratic_interpolation
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_domain_budget import PhaseResourceStopped
from cfd_reference3d_shared_factor import storage_sha


def interpolation_reserve(nt,nv,tet):
    if min(nt,nv,tet)<1:return 0
    return int(512*nt+128*tet+32*nv+32*2**20)


def csr_bytes(m):return int(m.indptr.nbytes+m.indices.nbytes+m.data.nbytes)


class P2Collector:
    def __init__(self,system,batch=64,stage_callback=None,pressure_control=True):
        self.callback=stage_callback;self.pressure_control=pressure_control;self.batch=batch
        nv=len(system.retained_free)-system.nmacro
        reserve=interpolation_reserve(system.nt,nv,system.mesh.nelements)
        if pressure_control:
            current=current_rss_bytes();record=dict(phase='p2_interpolation_preflight',current_rss_bytes=current,construction_reservation_bytes=reserve,reserve_bytes=32*2**20,estimated_stage_bytes=current+reserve+32*2**20,rss_cap_bytes=1800*2**20,wall_cap_s=180)
            print(json.dumps(record),flush=True)
            if record['estimated_stage_bytes']>1800*2**20:raise PhaseResourceStopped(record)
        self.Z,self.interpolation=macro_quadratic_interpolation(system,nv);self.nc=self.Z.shape[1]
        self.capacity=batch*30*31//2;self.rr=np.empty(self.capacity,dtype=np.int32);self.cc=np.empty(self.capacity,dtype=np.int32);self.values=np.empty(self.capacity)
        self.used=0;self.levels=[];self.macros=0;self.maximum_local_columns=0;self.maximum_skew=0.;self.max_live=0;self.max_temp=0
        if self.callback:self.callback('p2_interpolation_ready')
    def guard(self,leaf=None,new_nnz=0):
        arrays=sum(csr_bytes(m) for m in self.levels if m is not None)+(csr_bytes(leaf) if leaf is not None else 0)+self.rr.nbytes+self.cc.nbytes+self.values.nbytes+2**20
        predicted=12*new_nnz+8*(self.nc+1) if new_nnz else 0
        owned=arrays+predicted;self.max_live=max(self.max_live,owned)
        if owned>256*2**20:raise PhaseResourceStopped(dict(phase='p2_sparse_construction',owned_projected_bytes=owned,construction_cap_bytes=256*2**20,rss_cap_bytes=1800*2**20,wall_cap_s=180))
        if self.pressure_control and new_nnz:
            current=current_rss_bytes();total=current+2*predicted+32*2**20
            if total>1800*2**20:raise PhaseResourceStopped(dict(phase='p2_sparse_merge',current_rss_bytes=current,new_array_reservation_bytes=2*predicted,reserve_bytes=32*2**20,estimated_stage_bytes=total,rss_cap_bytes=1800*2**20,wall_cap_s=180))
    def push(self,leaf):
        level=0
        while level<len(self.levels) and self.levels[level] is not None:
            old=self.levels[level];self.guard(leaf,old.nnz+leaf.nnz);merged=old+leaf;self.levels[level]=None;leaf=merged;level+=1
        if level==len(self.levels):self.levels.append(None)
        self.levels[level]=leaf;self.guard()
    def add(self,S,keep,ids):
        selected=keep<102;iv=keep[selected];fine=ids[selected]
        P=self.Z[fine];columns=np.unique(P.indices)
        if len(columns)>30:raise ValueError('macro P2 projection exceeds30columns')
        self.maximum_local_columns=max(self.maximum_local_columns,len(columns))
        if len(columns):
            dense=P[:,columns].toarray();Sv=S[np.ix_(iv,iv)];upper=np.where(fine[:,None]<=fine[None,:],Sv,0.)
            V=upper+upper.T-np.diag(np.diag(upper));Ac=dense.T@V@dense
            skew=float(np.linalg.norm(Ac-Ac.T)/max(np.linalg.norm(Ac),1e-30));self.maximum_skew=max(self.maximum_skew,skew)
            if skew>1e-10:raise ValueError('local physical Galerkin asymmetry')
            Ac=(Ac+Ac.T)*.5;i,j=np.triu_indices(len(columns));end=self.used+len(i)
            if end>self.capacity:raise ValueError('coarse batch capacity exceeded')
            self.rr[self.used:end]=columns[i];self.cc[self.used:end]=columns[j];self.values[self.used:end]=Ac[i,j];self.used=end
            temp=dense.nbytes+Sv.nbytes+upper.nbytes+V.nbytes+Ac.nbytes+P.data.nbytes+P.indices.nbytes+P.indptr.nbytes+columns.nbytes+i.nbytes+j.nbytes
            self.max_temp=max(self.max_temp,temp)
            if temp>2**20:raise ValueError('local projection workspace bound exceeded')
        self.macros+=1
        if self.macros%self.batch==0:self.flush()
    def flush(self):
        if self.used:
            self.guard(new_nnz=self.used)
            leaf=coo_matrix((self.values[:self.used],(self.rr[:self.used],self.cc[:self.used])),shape=(self.nc,self.nc)).tocsr();self.push(leaf);self.used=0
        if self.callback:self.callback('p2_coarse_batch')
    def finish(self):
        self.flush();result=None
        for level in range(len(self.levels)-1,-1,-1):
            if self.levels[level] is None:continue
            old=self.levels[level]
            if result is None:result=old
            else:self.guard(result,result.nnz+old.nnz);result=result+old
            self.levels[level]=None
        if result is None:raise ValueError('empty P2 Galerkin operator')
        result.eliminate_zeros();result.sort_indices();self.upper=result
        del self.rr,self.cc,self.values,self.levels
        self.metadata=dict(kind='local_exact_macro_P2_velocity_Galerkin',interpolation=self.interpolation,coarse_dofs=self.nc,coarse_upper_nnz=result.nnz,coarse_input_array_bytes=csr_bytes(result),coarse_input_sha256=storage_sha(result.indptr,result.indices,result.data),maximum_local_columns=self.maximum_local_columns,maximum_local_relative_skew=self.maximum_skew,maximum_local_temporary_bytes=self.max_temp,maximum_projected_sparse_construction_bytes=self.max_live,macro_batch_cap=self.batch,macros=self.macros,physical_upper_orientation_retained=True,scope='exact projected original physical velocity Schur contributions; only roundoff symmetry restored in PC coarse matrix')
        if self.callback:self.callback('p2_coarse_assembly_complete')
