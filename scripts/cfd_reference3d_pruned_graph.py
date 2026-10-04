"""Optional graph approximation; complete original physical blocks stay authoritative."""
import numpy as np
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_shared_factor import storage_sha
THRESHOLDS=(0.,1e-3,1e-2)


def strength_data(triangle):
    if not isinstance(triangle,VectorTriangle) or not triangle.input_unchanged():raise ValueError('complete unchanged physical vector required')
    n=triangle.nodes;diagonal=triangle.starts[:-1]
    if not np.array_equal(triangle.rows[diagonal],np.arange(n)):raise ValueError('missing sorted diagonal blocks')
    blocks=triangle.values.reshape(-1,3,3)
    D=blocks[diagonal]
    if not np.array_equal(D,D.transpose(0,2,1)) or np.any(np.linalg.eigvalsh(D)<=0):raise ValueError('diagonal node block is not positive definite')
    scales=np.linalg.norm(D,axis=(1,2));roots=np.sqrt(scales)
    if not np.all(np.isfinite(scales)) or np.any(scales<=0):raise ValueError('invalid diagonal scale')
    norms=np.empty(len(triangle.rows));strength=np.empty_like(norms)
    for first in range(0,n,256):
        last=min(first+256,n);begin,end=triangle.starts[[first,last]]
        rows=np.repeat(np.arange(first,last),np.diff(triangle.starts[first:last+1]));cols=triangle.rows[begin:end]
        weights=np.linalg.norm(blocks[begin:end],axis=(1,2));denominator=roots[rows]*roots[cols]
        if not np.all(np.isfinite(weights)) or not np.all(np.isfinite(denominator)) or np.any(denominator<=0):raise ValueError('nonfinite strength')
        norms[begin:end]=weights;strength[begin:end]=weights/denominator
    if not np.all(np.isfinite(strength)):raise ValueError('nonfinite normalized strength')
    off=np.ones(len(norms),dtype=bool);off[diagonal]=False
    quantiles=(0.,.01,.05,.1,.25,.5,.75,.9,.95,.99,1.)
    statistics=dict(node_count=n,original_upper_blocks=len(norms),off_diagonal_blocks=int(off.sum()),diagonal_min_eigenvalue=float(np.linalg.eigvalsh(D).min()),strength_quantiles={str(q):float(v) for q,v in zip(quantiles,np.quantile(strength[off],quantiles))} if np.any(off) else {},exact_zero_off_diagonal_blocks=int(np.count_nonzero(off&(norms==0))),thresholds=[])
    for threshold in THRESHOLDS:
        drop=off&(strength<=threshold)
        statistics['thresholds'].append(dict(threshold=threshold,dropped_blocks=int(drop.sum()),retained_upper_blocks=int(len(norms)-drop.sum()),dropped_frobenius_weight_sum=float(norms[drop].sum())))
    return norms,strength,statistics


def compensated_input(triangle,threshold):
    if threshold not in THRESHOLDS:raise ValueError('unsupported declared graph threshold')
    norms,strength,statistics=strength_data(triangle);n=triangle.nodes
    keep=strength>threshold;keep[triangle.starts[:-1]]=True;weights=np.zeros(n)
    for first in range(0,n,256):
        last=min(first+256,n);begin,end=triangle.starts[[first,last]]
        nodes=np.repeat(np.arange(first,last),np.diff(triangle.starts[first:last+1]));drop=~keep[begin:end]
        np.add.at(weights,nodes[drop],norms[begin:end][drop]);np.add.at(weights,triangle.rows[begin:end][drop],norms[begin:end][drop])
    counts=np.add.reduceat(keep.astype(np.int64),triangle.starts[:-1]);starts=np.r_[0,np.cumsum(counts)].astype(np.int64)
    rows=np.array(triangle.rows[keep],dtype=np.int32,copy=True);rounded=np.empty(9*len(rows),dtype=np.float32)
    source_indices=np.flatnonzero(keep);source=triangle.values.reshape(-1,3,3);diagonal=triangle.starts[:-1]
    for first in range(0,len(rows),4096):
        last=min(first+4096,len(rows));ids=source_indices[first:last];v=source[ids].copy()
        positions=np.searchsorted(diagonal,ids);isdiag=(positions<n)&(diagonal[np.minimum(positions,n-1)]==ids)
        if np.any(isdiag):v[isdiag]+=weights[positions[isdiag],None,None]*np.eye(3)[None,:,:]
        with np.errstate(over='ignore',under='ignore'):f=v.astype(np.float32)
        if not np.all(np.isfinite(f)) or np.any((v!=0)&(f==0)):raise ValueError('preconditioner Float overflow/underflow')
        rounded[9*first:9*last]=f.ravel()
    if not triangle.input_unchanged():raise ValueError('approximation changed physical input')
    metadata=dict(threshold=threshold,statistics=statistics,retained_upper_blocks=len(rows),dropped_upper_blocks=len(triangle.rows)-len(rows),diagonal_compensation_sum=float(weights.sum()),maximum_node_compensation=float(weights.max()),input_allocation_bytes=starts.nbytes+rows.nbytes+rounded.nbytes,compensation='Frobenius weight times identity on both incident diagonal node blocks',physical_input_preserved_bitwise=True,physical_input_sha256=triangle.input_sha256,rounded_input_sha256=storage_sha(starts,rows,rounded),scope='approximate preconditioner only; original Float64 physical operator and every pressure mode unchanged')
    return starts,rows,rounded,metadata
