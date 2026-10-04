"""Fixed-pattern block IC0 and balanced physical velocity correction; PC only."""
import ctypes as ct
import hashlib,json
from pathlib import Path
import numpy as np
from scipy.linalg import cho_factor,cho_solve
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_allocator_pressure import release_free_pages,current_rss_bytes


def velocity_reserve(nv):
    if not isinstance(nv,int) or nv<30:raise ValueError('invalid velocity reservation')
    return 8*(4*nv*30+20*nv+8*30**2)+2*1024**2


def fresh_admission(factor,outer_bytes,pressure_bytes,rss_reader=current_rss_bytes):
    if any(not isinstance(x,int) or x<0 for x in (outer_bytes,pressure_bytes)):raise ValueError('invalid reservations')
    fresh=rss_reader()
    if not isinstance(fresh,int) or fresh<=0 or not factor.pressure_record:raise ValueError('fresh pressure stage required')
    a=dict(current_rss_before_numeric_bytes=fresh,earlier_post_relief_rss_bytes=factor.pressure_record['current_rss_after_bytes'],factor_storage_bytes=72*len(factor.rows)+1024,numeric_workspace_bytes=32*factor.n,reserve_bytes=32*1024**2,outer_basis_reservation_bytes=outer_bytes,coarse_pressure_reservation_bytes=pressure_bytes,coarse_velocity_reservation_bytes=velocity_reserve(factor.n))
    a['basis_reservation_bytes']=outer_bytes+pressure_bytes+a['coarse_velocity_reservation_bytes']
    a['estimated_numeric_stage_bytes']=fresh+a['factor_storage_bytes']+a['numeric_workspace_bytes']+a['reserve_bytes']+a['basis_reservation_bytes']
    a['numeric_stage_admitted']=a['estimated_numeric_stage_bytes']<=1800*1024**2
    a['residency_measurement']='fresh after physical-input validation; complete bases, both coarse spaces and solve scratch reserved'
    return a


def pattern(triangle,library,check=None,max_work=384*1024**2):
    if not isinstance(max_work,int) or max_work<0 or not triangle.input_unchanged():raise ValueError('invalid pattern inputs')
    n=triangle.nodes;degree=np.diff(triangle.starts)+np.bincount(triangle.rows,minlength=n)-2
    perm=np.lexsort((np.arange(n),degree)).astype(np.int32);inverse=np.empty(n,dtype=np.int32);inverse[perm]=np.arange(n,dtype=np.int32)
    lib=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int)
    pre=lib.cfd_fill1_workspace;pre.argtypes=(ct.c_int,lp,ip,ip,lp,ct.POINTER(ct.c_size_t),lp);pre.restype=ct.c_int
    create=lib.cfd_fill1_pattern_create;create.argtypes=(ct.c_int,lp,ip,ip,ct.c_size_t,ip);create.restype=ct.c_void_p
    destroy=lib.cfd_fill1_pattern_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
    for name,typ in (('starts',lp),('rows',ip),('blocks',ct.c_long),('complete_fill',ct.c_long),('kept_fill',ct.c_long),('workspace',ct.c_size_t)):
        f=getattr(lib,'cfd_fill1_pattern_'+name);f.argtypes=(ct.c_void_p,);f.restype=typ
    counts=np.empty(n,dtype=np.int64);need=ct.c_size_t();pairs=ct.c_long();args=(n,triangle.starts.ctypes.data_as(lp),triangle.rows.ctypes.data_as(ip),inverse.ctypes.data_as(ip))
    status=pre(*args,counts.ctypes.data_as(lp),ct.byref(need),ct.byref(pairs))
    if status:raise ValueError('pattern preflight rejected: '+str(status))
    metadata=dict(kind='truncated_original_edge_level_one_fill',ordering='ascending_symmetric_degree_then_original_node',original_blocks=len(triangle.rows),pattern_block_cap=2*len(triangle.rows)-n,candidate_pairs=pairs.value,construction_workspace_bound_bytes=need.value,construction_workspace_limit_bytes=max_work,permutation_sha256=storage_sha(perm,inverse))
    if check:check(metadata)
    if need.value>max_work:raise ValueError('pattern construction workspace bound exceeded')
    status=ct.c_int(-100);handle=None
    try:
        handle=create(*args,max_work,ct.byref(status))
        if not handle or status.value:raise ValueError('pattern construction rejected: '+str(status.value))
        blocks=int(lib.cfd_fill1_pattern_blocks(handle));starts=np.ctypeslib.as_array(lib.cfd_fill1_pattern_starts(handle),shape=(n+1,)).copy();rows=np.ctypeslib.as_array(lib.cfd_fill1_pattern_rows(handle),shape=(blocks,)).copy()
        metadata.update(pattern_blocks=blocks,complete_level_one_fill=int(lib.cfd_fill1_pattern_complete_fill(handle)),kept_fill=int(lib.cfd_fill1_pattern_kept_fill(handle)),pattern_array_bytes=starts.nbytes+rows.nbytes,permutation_array_bytes=perm.nbytes+inverse.nbytes)
        metadata['discarded_level_one_fill']=metadata['complete_level_one_fill']-metadata['kept_fill']
        if blocks>metadata['pattern_block_cap'] or starts[-1]!=blocks or not triangle.input_unchanged():raise ValueError('pattern bounds/input preservation failed')
        return starts,rows,perm,inverse,metadata
    finally:
        if handle:destroy(handle)


class BlockIC0:
    def __init__(self,triangle,library,pressure_control=True,live_arrays=(),action=None,stage_callback=None):
        self._handle=None;self.owner=triangle;self.n=triangle.shape[0]
        if self.n!=3*triangle.nodes or not triangle.input_unchanged():raise ValueError('invalid physical triangle')
        self.original_starts=triangle.starts;self.original_rows=triangle.rows
        self.rounded=triangle.predictor if hasattr(triangle,'predictor') else triangle.values.astype(np.float32)

        self.library=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);fp=ct.POINTER(ct.c_float);dp=ct.POINTER(ct.c_double)
        create=self.library.cfd_fill1_factor_create;create.argtypes=(ct.c_int,ct.c_long,lp,ip,lp,ip,fp,ip,ip);create.restype=ct.c_void_p
        solve=self.library.cfd_reference_ic0_solve;solve.argtypes=(ct.c_void_p,dp,dp,dp);solve.restype=ct.c_int
        destroy=self.library.cfd_reference_ic0_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        for name,typ in (('fill_pairs',ct.c_long),('fill_sum',ct.c_double),('fill_max',ct.c_double),('storage',ct.c_size_t),('shifted',ct.c_long),('shift_sum',ct.c_double),('shift_max',ct.c_double),('min_pivot',ct.c_double),('values',dp)):
            f=getattr(self.library,'cfd_reference_ic0_'+name);f.argtypes=(ct.c_void_p,);f.restype=typ
        def construction_check(meta):
            from cfd_reference3d_domain_budget import PhaseResourceStopped
            current=current_rss_bytes();projection=current+meta['construction_workspace_bound_bytes']+32*1024**2
            record=dict(phase='fill1_symbolic_preflight',**meta,current_rss_bytes=current,estimated_stage_bytes=projection,rss_cap_bytes=1800*1024**2,wall_cap_s=180)
            print(json.dumps(record),flush=True)
            if projection>1800*1024**2 or meta['construction_workspace_bound_bytes']>meta['construction_workspace_limit_bytes']:raise PhaseResourceStopped(record)
            if stage_callback:stage_callback('fill1_symbolic_preflight')
        self.starts,self.rows,self.perm,self.inverse_permutation,self.pattern_metadata=pattern(triangle,library,construction_check if pressure_control else None)
        self.input_sha256=storage_sha(self.starts,self.rows,self.rounded,self.perm,self.inverse_permutation)
        try:
            if stage_callback:stage_callback('workspace_symbolic_ready')
            self.pressure_record=release_free_pages((self.starts,self.rows,self.rounded,self.perm,self.inverse_permutation,*live_arrays),action) if pressure_control else None
            if stage_callback:stage_callback('workspace_pressure_complete')
            status=ct.c_int(-100)
            self._handle=create(triangle.nodes,len(self.rows),self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.original_starts.ctypes.data_as(lp),self.original_rows.ctypes.data_as(ip),self.rounded.ctypes.data_as(fp),self.inverse_permutation.ctypes.data_as(ip),ct.byref(status))
            if not self._handle or status.value:raise ValueError('block IC0 rejected: '+str(status.value))
            if not self.input_unchanged():raise ValueError('factor altered physical input')
            self.metadata=dict(kind='bounded_degree_ordered_fill1',pattern=self.pattern_metadata,fill_compensated_pairs=int(self.library.cfd_reference_ic0_fill_pairs(self._handle)),fill_diagonal_compensation_sum=float(self.library.cfd_reference_ic0_fill_sum(self._handle)),fill_diagonal_compensation_max=float(self.library.cfd_reference_ic0_fill_max(self._handle)),ordering='ascending_symmetric_degree_then_original_node',block_size=3,factor_dtype='float64',predictor_dtype='float32',factor_storage_bytes=int(self.library.cfd_reference_ic0_storage(self._handle)),factor_storage_bound_bytes=72*len(self.rows)+1024,solve_workspace_bytes=32*self.n,shifted_pivots=int(self.library.cfd_reference_ic0_shifted(self._handle)),pivot_shift_sum=float(self.library.cfd_reference_ic0_shift_sum(self._handle)),pivot_shift_max=float(self.library.cfd_reference_ic0_shift_max(self._handle)),minimum_factor_pivot=float(self.library.cfd_reference_ic0_min_pivot(self._handle)),rounded_values_sha256=storage_sha(self.rounded),library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),pressure_control=self.pressure_record,physical_operator_unchanged=True,scope='bounded extra fill with degree coordinate bijection; omitted-fill/pivot compensation confined to preconditioner; full Float64 physical action retained')
            if self.metadata['factor_storage_bytes']>self.metadata['factor_storage_bound_bytes']:raise ValueError('factor bound exceeded')
            if stage_callback:stage_callback('workspace_numeric_ready')
        except BaseException:self.close();raise
    def input_unchanged(self):return self.owner.input_unchanged() and storage_sha(self.starts,self.rows,self.rounded,self.perm,self.inverse_permutation)==self.input_sha256
    def solve(self,x):
        rhs=np.ascontiguousarray(x,dtype=float)
        if not self._handle or rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid/live factor RHS required')
        rhs=np.ascontiguousarray(rhs.reshape(3,self.owner.nodes)[:,self.perm]).ravel()
        out=np.empty(self.n);work=np.empty(self.n);dp=ct.POINTER(ct.c_double)
        if self.library.cfd_reference_ic0_solve(self._handle,rhs.ctypes.data_as(dp),out.ctypes.data_as(dp),work.ctypes.data_as(dp)):raise ValueError('IC solve rejected')
        physical=np.empty((3,self.owner.nodes));physical[:,self.perm]=out.reshape(3,self.owner.nodes)
        return physical.ravel()
    def lower_copy(self):
        if not self._handle:raise ValueError('closed factor')
        return np.ctypeslib.as_array(self.library.cfd_reference_ic0_values(self._handle),shape=(9*len(self.rows),)).copy()
    def close(self):
        if self._handle:self.library.cfd_reference_ic0_destroy(self._handle);self._handle=None


class BalancedVelocity:
    def __init__(self,Z,Y,inverse):
        if Z.ndim!=2 or Y.shape!=Z.shape or not all(np.all(np.isfinite(a)) for a in (Z,Y)):raise ValueError('invalid velocity coarse data')
        c=Z.T@Y;skew=float(np.linalg.norm(c-c.T)/max(np.linalg.norm(c),1e-30))
        if skew>1e-10:raise ValueError('physical coarse asymmetry')
        c=(c+c.T)/2;eig=np.linalg.eigvalsh(c)
        if eig[0]<=eig[-1]*1e-12:raise ValueError('rank deficient velocity coarse space')
        self.Z=Z;self.Y=Y;self.factor=cho_factor(c,lower=True);self.inverse=inverse
        reproduction=float(np.linalg.norm(np.column_stack([self.solve(col) for col in Y.T])-Z)/max(np.linalg.norm(Z),1e-30))
        if reproduction>1e-8:raise ValueError('velocity coarse reproduction failed: '+str(reproduction))
        self.metadata=dict(kind='balanced_quadratic_global_velocity30',columns=Z.shape[1],coarse_relative_skew=skew,coarse_reproduction_relative_error=reproduction,coarse_smallest_eigenvalue=float(eig[0]),coarse_largest_eigenvalue=float(eig[-1]),owned_coarse_array_bytes=Z.nbytes+Y.nbytes+self.factor[0].nbytes)
    def solve(self,x):
        q=self.Z.T@x;a=cho_solve(self.factor,q,check_finite=False)
        t=self.inverse(x-self.Y@a)
        return t+self.Z@cho_solve(self.factor,q-self.Y.T@t,check_finite=False)


def basis(coords,length,lo,hi):
    if coords.ndim!=2 or coords.shape[1]!=3 or not np.all(np.isfinite(coords)):raise ValueError('invalid free trace coordinates')
    x,y,z=(2*coords/np.array([length,2.,2.])-1).T
    wall=coords[:,1]*(2-coords[:,1])*coords[:,2]*(2-coords[:,2])
    distance=np.linalg.norm(np.maximum(np.maximum(lo-coords,coords-hi),0),axis=1)
    weight=wall*np.minimum(distance/.2,1.)
    raw=weight[:,None]*np.column_stack((np.ones(len(x)),x,y,z,x*x,y*y,z*z,x*y,x*z,y*z))
    q,r=np.linalg.qr(raw,mode='reduced')
    if q.shape[1]!=10 or np.min(np.abs(np.diag(r)))<=np.linalg.norm(r)*1e-12:raise ValueError('rank deficient trace polynomials')
    Z=np.zeros((3*len(coords),30))
    for a in range(3):Z[a*len(coords):(a+1)*len(coords),a*10:(a+1)*10]=q
    return Z


class BalancedIC0(BlockIC0):
    def __init__(self,triangle,library,ordering='metis',pressure_control=True,live_arrays=(),action=None,stage_callback=None,coords=None,length=None,lo=None,hi=None):
        super().__init__(triangle,library,pressure_control,live_arrays,action,stage_callback)
        try:
            print(json.dumps(dict(phase='fill1_factor_report',**self.metadata)),flush=True)
            Z=basis(coords,length,lo,hi);Y=triangle@Z
            self.coarse=BalancedVelocity(Z,Y,lambda x:BlockIC0.solve(self,x))
            self.metadata.update(velocity_coarse=self.coarse.metadata,coarse_velocity_reservation_bytes=velocity_reserve(self.n))
            if stage_callback:stage_callback('coarse_velocity_ready')
        except BaseException:self.close();raise
    def solve(self,x):
        if not self._handle:raise ValueError('closed balanced factor')
        return self.coarse.solve(x)
    def close(self):
        if hasattr(self,'coarse'):del self.coarse
        super().close()
