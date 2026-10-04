"""Bounded streamed graph selection from one live, preserved seed factor."""
import ctypes as ct
import json
from pathlib import Path
import numpy as np
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_domain_budget import PhaseResourceStopped

def seed_pattern(seed,library,work_reservation_bytes,max_work=384*2**20,stage_callback=None):
    if not isinstance(work_reservation_bytes,int) or work_reservation_bytes<0 or not isinstance(max_work,int) or max_work<0 or not seed._handle or not seed.input_unchanged():raise ValueError('live preserved seed and valid reservations required')
    lib=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);fp=ct.POINTER(ct.c_float)
    pre=lib.cfd_seed_fill1_workspace;pre.argtypes=(ct.c_void_p,ct.c_int,lp,ip,ip,lp,ct.POINTER(ct.c_size_t),lp);pre.restype=ct.c_int
    create=lib.cfd_seed_fill1_pattern_create;create.argtypes=(ct.c_void_p,ct.c_int,lp,ip,ip,fp,ct.c_size_t,ip);create.restype=ct.c_void_p
    destroy=lib.cfd_fill1_pattern_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
    for name,typ in (('starts',lp),('rows',ip),('blocks',ct.c_long),('complete_fill',ct.c_long),('kept_fill',ct.c_long),('workspace',ct.c_size_t)):
        f=getattr(lib,'cfd_fill1_pattern_'+name);f.argtypes=(ct.c_void_p,);f.restype=typ
    nodes=seed.owner.nodes;counts=np.empty(nodes,dtype=np.int64);need=ct.c_size_t();pairs=ct.c_long()
    args=(seed._handle,nodes,seed.original_starts.ctypes.data_as(lp),seed.original_rows.ctypes.data_as(ip),seed.inverse_permutation.ctypes.data_as(ip))
    status=pre(*args,counts.ctypes.data_as(lp),ct.byref(need),ct.byref(pairs))
    if status:raise ValueError('seed graph preflight rejected: '+str(status))
    def seed_values_sha():return storage_sha(np.ctypeslib.as_array(seed.library.cfd_reference_ic0_values(seed._handle),shape=(9*len(seed.rows),)))
    before=seed_values_sha();current=current_rss_bytes();projection=current+need.value+32*2**20+work_reservation_bytes
    metadata=dict(kind='streamed_seed_elimination_strength_fill',ranking='sum normalized seed lower-block norm products; original edges mandatory; descending score then row index',original_blocks=len(seed.original_rows),pattern_block_cap=2*len(seed.original_rows)-nodes,seed_factor_blocks=len(seed.rows),seed_candidate_pairs=pairs.value,seed_input_sha256=seed.input_sha256,seed_factor_values_sha256=before,ranking_predictor_sha256=storage_sha(seed.rounded),permutation_sha256=storage_sha(seed.perm,seed.inverse_permutation),construction_workspace_bound_bytes=need.value,construction_workspace_limit_bytes=max_work)
    record=dict(phase='seed_graph_symbolic_preflight',**metadata,current_rss_bytes=current,complete_work_reservation_bytes=work_reservation_bytes,reserve_bytes=32*2**20,estimated_stage_bytes=projection,rss_cap_bytes=1800*2**20,wall_cap_s=180)
    print(json.dumps(record),flush=True)
    if need.value>max_work or projection>1800*2**20:raise PhaseResourceStopped(record)
    if stage_callback:stage_callback('seed_graph_symbolic_preflight')
    status=ct.c_int(-100);handle=None
    try:
        handle=create(*args,seed.rounded.ctypes.data_as(fp),max_work,ct.byref(status))
        if not handle or status.value:raise ValueError('seed graph construction rejected: '+str(status.value))
        blocks=int(lib.cfd_fill1_pattern_blocks(handle));starts=np.ctypeslib.as_array(lib.cfd_fill1_pattern_starts(handle),shape=(nodes+1,)).copy();rows=np.ctypeslib.as_array(lib.cfd_fill1_pattern_rows(handle),shape=(blocks,)).copy()
        if blocks>metadata['pattern_block_cap'] or starts[-1]!=blocks or not seed.input_unchanged() or seed_values_sha()!=before:raise ValueError('seed graph bounds/input preservation failed')
        metadata.update(pattern_blocks=blocks,generated_fill=int(lib.cfd_fill1_pattern_complete_fill(handle)),kept_fill=int(lib.cfd_fill1_pattern_kept_fill(handle)),pattern_array_bytes=starts.nbytes+rows.nbytes,permutation_array_bytes=seed.perm.nbytes+seed.inverse_permutation.nbytes,pattern_sha256=storage_sha(starts,rows),seed_factor_preserved=True)
        if stage_callback:stage_callback('seed_graph_ready')
        return starts,rows,seed.perm,seed.inverse_permutation,metadata
    finally:
        if handle:destroy(handle)
