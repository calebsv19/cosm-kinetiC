"""Sparse distributed balanced velocity correction; both numeric factors guarded."""
import ctypes as ct,hashlib,json
from cfd_reference3d_energy_cg8 import energy_cg8,new_statistics
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
from cfd_reference3d_bounded_fill1 import BlockIC0
from cfd_reference3d_shared_factor import validate_csr,storage_sha
from cfd_reference3d_triangle import validate_upper
from cfd_reference3d_allocator_pressure import current_rss_bytes


def work_reserve(nv,nc):
    if not isinstance(nv,int) or not isinstance(nc,int) or min(nv,nc)<1:raise ValueError('invalid distributed reservation')
    return int(8*(40*nv+24*nc)+2*2**20)


def triple(action,inverse,x):
    rhs=np.asarray(x,dtype=float)
    u=inverse(rhs)
    for _ in range(2):u=u+inverse(rhs-action(u))
    if not np.all(np.isfinite(u)):raise ValueError('nonfinite fixed residual correction')
    return u


def inner_cg8(action,precondition,x):
    rhs=np.asarray(x,dtype=float)
    if rhs.ndim!=1 or not np.all(np.isfinite(rhs)):raise ValueError('invalid inner CG RHS')
    u=np.zeros_like(rhs);r=rhs.copy()
    if not np.any(r):return u
    z=precondition(r);p=z.copy();rz=float(r@z)
    if not np.isfinite(rz) or rz<=0:raise ValueError('inner CG nonpositive preconditioner')
    for iteration in range(8):
        Ap=action(p);curvature=float(p@Ap)
        if not np.isfinite(curvature) or curvature<=0:raise ValueError('inner CG nonpositive physical curvature')
        alpha=rz/curvature;u=u+alpha*p;r=r-alpha*Ap
        if not np.all(np.isfinite(u)) or not np.all(np.isfinite(r)):raise ValueError('nonfinite inner CG')
        if iteration==7 or not np.any(r):break
        z=precondition(r);next_rz=float(r@z)
        if not np.isfinite(next_rz) or next_rz<=0:raise ValueError('inner CG nonpositive preconditioner')
        p=z+(next_rz/rz)*p;rz=next_rz
    return u


class RefinedFloatCoarse:
    def __init__(self,upper,library,numeric=True,ordering='metis'):
        self._handle=None;validate_csr(upper);validate_upper(upper)
        if ordering not in ('amd','metis'):raise ValueError('invalid ordering')
        self.owner=upper;self.n=upper.shape[0];self.starts=upper.indptr.astype(np.int64);self.rows=upper.indices;self.values=upper.data.astype(np.float32)
        if not np.all(np.isfinite(self.values)) or np.any((upper.data!=0)&(self.values==0)):raise ValueError("invalid Float coarse predictor")
        self.physical_sha256=storage_sha(upper.indptr,upper.indices,upper.data)
        self.input_sha256=storage_sha(self.starts,self.rows,self.values);self.library=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);fp=ct.POINTER(ct.c_float)
        self.scalar_action=self.library.cfd_reference_coarse_scalar_action;self.scalar_action.argtypes=(ct.c_int,lp,ip,dp,dp,dp);self.scalar_action.restype=ct.c_int
        create=self.library.cfd_reference_factor_create_symbolic;create.argtypes=(ct.c_int,lp,ip,fp,ct.c_int,ct.c_int,ip);create.restype=ct.c_void_p
        numeric_fn=self.library.cfd_reference_factor_numeric;numeric_fn.argtypes=(ct.c_void_p,);numeric_fn.restype=ct.c_int
        solve=self.library.cfd_reference_factor_solve;solve.argtypes=(ct.c_void_p,dp,dp);solve.restype=ct.c_int
        destroy=self.library.cfd_reference_factor_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        for name in ('storage','numeric_workspace','workspace'):
            f=getattr(self.library,'cfd_reference_factor_'+name);f.argtypes=(ct.c_void_p,);f.restype=ct.c_size_t
        user=self.library.cfd_reference_factor_user_storage;user.argtypes=(ct.c_void_p,);user.restype=ct.c_int
        try:
            status=ct.c_int(-100);self._handle=create(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.values.ctypes.data_as(fp),3 if ordering=='metis' else 2,1,ct.byref(status))
            if not self._handle or status.value:raise ValueError('Float coarse symbolic rejected: '+str(status.value))
            self.numeric_ready=False
            self.metadata=dict(kind='Float_coarse_workspace_cholesky_three_physical_residual_steps',dofs=self.n,ordering=ordering,symbolic_factor_storage_bytes=self.storage,numeric_workspace_bytes=self.numeric_workspace,input_sha256=self.input_sha256,library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),value_dtype='float32',physical_value_dtype='float64',coarse_action_backend='one_pass_native_original_Double_upper_CSR',scalar_action_abi_arguments=6,fixed_residual_steps=3,block_size=1,physical_sha256=self.physical_sha256,predictor_array_bytes=self.values.nbytes,numeric_admission_required=True)
            if numeric:self.numeric()
        except BaseException:self.close();raise
    @property
    def storage(self):return int(self.library.cfd_reference_factor_storage(self._handle))
    @property
    def numeric_workspace(self):return int(self.library.cfd_reference_factor_numeric_workspace(self._handle))
    def numeric(self):
        if not self._handle or self.numeric_ready:raise ValueError('live unnumebered coarse factor required')
        status=self.library.cfd_reference_factor_numeric(self._handle)
        if status:raise ValueError('exact coarse numeric rejected: '+str(status))
        self.numeric_ready=True;workspace=int(self.library.cfd_reference_factor_workspace(self._handle));self.metadata.update(numeric_status=status,solve_workspace_bytes=workspace,user_factor_storage_verified=bool(self.library.cfd_reference_factor_user_storage(self._handle)))
        if not self.metadata['user_factor_storage_verified'] or not self.input_unchanged():raise ValueError('coarse ownership/input changed')
    def input_unchanged(self):return self.input_sha256==storage_sha(self.starts,self.rows,self.values) and self.physical_sha256==storage_sha(self.owner.indptr,self.owner.indices,self.owner.data)
    def base_solve(self,x):
        rhs=np.ascontiguousarray(x,dtype=float)
        if not self._handle or not self.numeric_ready or rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid live coarse RHS')
        result=np.empty(self.n);dp=ct.POINTER(ct.c_double)
        if self.library.cfd_reference_factor_solve(self._handle,rhs.ctypes.data_as(dp),result.ctypes.data_as(dp)):raise ValueError('coarse solve rejected')
        return result
    def solve(self,x):return triple(self.action,self.base_solve,x)
    def action(self,x):
        rhs=np.ascontiguousarray(x,dtype=float)
        if rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid scalar coarse action RHS')
        result=np.empty(self.n);lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double)
        if self.scalar_action(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.owner.data.ctypes.data_as(dp),rhs.ctypes.data_as(dp),result.ctypes.data_as(dp)):raise ValueError('native coarse scalar action rejected')
        return result
    def close(self):
        if self._handle:self.library.cfd_reference_factor_destroy(self._handle);self._handle=None


class BalancedSparse:
    def __init__(self,velocity,Z,coarse,inverse):
        if not isinstance(Z,csr_matrix) or Z.shape[0]!=velocity.shape[0] or Z.shape[1]!=coarse.n or not np.all(np.isfinite(Z.data)):raise ValueError('invalid distributed basis')
        self.velocity=velocity;self.Z=Z;self.coarse=coarse;self.inverse=inverse;self.sha=storage_sha(Z.indptr,Z.indices,Z.data)
        errors=[];actions=[];rng=np.random.default_rng(985271)
        for i in range(3):
            s=rng.normal(size=Z.shape[1]);u=Z@s;rhs=velocity@u;expected=coarse.action(s);actual=Z.T@rhs
            actions.append(float(np.linalg.norm(actual-expected)/max(np.linalg.norm(expected),1e-30)))
            errors.append(float(np.linalg.norm(self.solve(rhs)-u)/max(np.linalg.norm(u),1e-30)))
        if max(actions)>1e-10 or max(errors)>1e-8:raise ValueError('distributed coarse action/reproduction failed: '+str((actions,errors)))
        self.metadata=dict(kind='balanced_macro_P3_velocity_three_physical_residual_steps',coarse_columns=Z.shape[1],sampled_coarse_action_relative_errors=actions,sampled_coarse_reproduction_relative_errors=errors,sampled_vectors=3,full_spectrum_certificate=False,interpolation_sha256=self.sha,interpolation_array_bytes=Z.indptr.nbytes+Z.indices.nbytes+Z.data.nbytes)
    def solve(self,x):
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=self.velocity.shape[:1] or not np.all(np.isfinite(rhs)):raise ValueError('invalid balanced RHS')
        coarse=self.Z@self.coarse.solve(self.Z.T@rhs)
        local=self.inverse(rhs-self.velocity@coarse)
        return coarse+local-self.Z@self.coarse.solve(self.Z.T@(self.velocity@local))


class AdaptiveCG8Factor(BlockIC0):
    def __init__(self,triangle,library,ordering='metis',pressure_control=True,live_arrays=(),action=None,stage_callback=None,coords=None,length=None,lo=None,hi=None,coarse_library=None,Z=None,coarse_upper=None,coarse_metadata=None):
        self._handle=None;self.coarse_factor=None;self.Z=Z;self.inner_statistics=new_statistics()
        if not isinstance(Z,csr_matrix) or Z.shape[0]!=triangle.shape[0]:raise ValueError('invalid distributed input')
        self.Z_sha256=storage_sha(Z.indptr,Z.indices,Z.data)
        try:
            self.coarse_factor=RefinedFloatCoarse(coarse_upper,coarse_library,numeric=False,ordering=ordering)
            if stage_callback:stage_callback('p3_coarse_symbolic_ready')
            inputs=(Z.indptr,Z.indices,Z.data,coarse_upper.indptr,coarse_upper.indices,coarse_upper.data,self.coarse_factor.starts,self.coarse_factor.values,*live_arrays)
            super().__init__(triangle,library,pressure_control,inputs,action,stage_callback)
            self.coarse_factor.numeric()
            if self.coarse_factor.metadata['solve_workspace_bytes']>work_reserve(self.n,Z.shape[1]):raise ValueError('coarse solve workspace reservation exceeded')
            if stage_callback:stage_callback('p3_coarse_numeric_ready')
            self.balanced=BalancedSparse(triangle,Z,self.coarse_factor,lambda x:energy_cg8(lambda u:triangle@u,lambda r:BlockIC0.solve(self,r),x,stats=self.inner_statistics))
            local_metadata=self.metadata.copy();self.metadata=dict(kind='P3_controlled_fill_adaptive_CG8_energy25',inner_statistics=self.inner_statistics,inner_proxy_eta=.25,local_inner_iteration_cap=8,coarse_fixed_residual_steps=3,spd_claim='physical velocity and local factor SPD models; nonlinear inner CG requires flexible outer; no exact linear or spectral claim',local_factor=local_metadata,coarse_factor=self.coarse_factor.metadata,distributed_velocity=self.balanced.metadata,coarse_assembly=coarse_metadata,rounded_values_sha256=local_metadata['rounded_values_sha256'],factor_storage_bytes=local_metadata['factor_storage_bytes']+self.coarse_factor.storage,distributed_work_reservation_bytes=work_reserve(self.n,Z.shape[1]),scope='sparse physical Galerkin plus separately refined Float coarse and controlled-fill local inverses; full original mixed equations and pressure modes retained')
            if stage_callback:stage_callback('distributed_velocity_ready')
        except BaseException:self.close();raise
    def input_unchanged(self):return BlockIC0.input_unchanged(self) and self.coarse_factor.input_unchanged() and storage_sha(self.Z.indptr,self.Z.indices,self.Z.data)==self.Z_sha256
    def solve(self,x):
        if not self._handle:raise ValueError('closed distributed factor')
        return self.balanced.solve(x)
    def close(self):
        if hasattr(self,'balanced'):del self.balanced
        BlockIC0.close(self)
        if self.coarse_factor is not None:self.coarse_factor.close()


def fresh_admission(factor,outer_bytes,pressure_bytes,rss_reader=current_rss_bytes):
    if any(not isinstance(x,int) or x<0 for x in (outer_bytes,pressure_bytes)):raise ValueError('invalid reservation')
    fresh=rss_reader()
    if not isinstance(fresh,int) or fresh<=0 or not factor.pressure_record:raise ValueError('fresh pressure stage required')
    local=72*len(factor.rows)+1024;coarse=factor.coarse_factor.storage;scratch=max(32*factor.n,factor.coarse_factor.numeric_workspace);work=work_reserve(factor.n,factor.Z.shape[1])
    a=dict(current_rss_before_numeric_bytes=fresh,earlier_post_relief_rss_bytes=factor.pressure_record['current_rss_after_bytes'],local_factor_storage_bound_bytes=local,coarse_factor_storage_bytes=coarse,factor_storage_bytes=local+coarse,numeric_workspace_bytes=scratch,coarse_numeric_workspace_bytes=factor.coarse_factor.numeric_workspace,local_solve_workspace_bytes=32*factor.n,reserve_bytes=32*2**20,outer_basis_reservation_bytes=outer_bytes,coarse_pressure_reservation_bytes=pressure_bytes,distributed_velocity_work_reservation_bytes=work,basis_reservation_bytes=outer_bytes+pressure_bytes+work)
    a['estimated_numeric_stage_bytes']=fresh+local+coarse+scratch+a['reserve_bytes']+a['basis_reservation_bytes'];a['numeric_stage_admitted']=a['estimated_numeric_stage_bytes']<=1800*2**20
    return a
