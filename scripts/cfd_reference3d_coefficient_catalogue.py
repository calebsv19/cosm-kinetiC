"""Lossless float64 bit-pattern catalogue with bounded batch work and stream proof."""
import hashlib
import numpy as np
from cfd_reference3d_shared_factor import storage_sha

def encode(values,batch=262144,check=None):
    if not isinstance(values,np.ndarray) or values.ndim!=1 or values.dtype!=np.dtype('float64') or not values.flags.c_contiguous or not 1<=len(values)<=50000000 or not isinstance(batch,int) or not 1<=batch<=262144:raise ValueError('invalid coefficient codec input')
    original=storage_sha(values);source=values.view(np.uint64);catalogue=np.empty(0,dtype=np.uint64);sorted_words=np.empty(0,dtype=np.uint64);sorted_ids=np.empty(0,dtype=np.uint32)
    if check:check('catalogue_ids_allocation',4*len(values)+24*batch)
    codes=np.empty(len(values),dtype=np.uint32);maximum_unique=0;batches=0
    for begin in range(0,len(values),batch):
        if check:check('catalogue_batch',24*min(batch,len(values)-begin))
        unique,inverse=np.unique(source[begin:begin+batch],return_inverse=True);positions=np.searchsorted(sorted_words,unique);known=positions<len(sorted_words)
        known[known]&=sorted_words[positions[known]]==unique[known];new=unique[~known];next_count=len(catalogue)+len(new)
        if next_count>np.iinfo(np.uint32).max:raise ValueError('catalogue index overflow')
        if check:check('catalogue_merge',40*next_count+24*len(unique))
        local=np.empty(len(unique),dtype=np.uint32);local[known]=sorted_ids[positions[known]]
        if len(new):
            new_ids=np.arange(len(catalogue),next_count,dtype=np.uint32);local[~known]=new_ids
            catalogue=np.concatenate((catalogue,new));all_words=np.concatenate((sorted_words,new));all_ids=np.concatenate((sorted_ids,new_ids));order=np.argsort(all_words)
            sorted_words=all_words[order];sorted_ids=all_ids[order];del all_words,all_ids,order,new_ids
        codes[begin:begin+len(inverse)]=local[inverse];maximum_unique=max(maximum_unique,len(unique));batches+=1
        del unique,inverse,positions,known,new,local
        if check:check('catalogue_batch_complete',0)
    del sorted_words,sorted_ids
    h=hashlib.sha256();h.update(str((values.shape,values.dtype.str)).encode())
    for begin in range(0,len(values),batch):
        decoded=catalogue[codes[begin:begin+batch]]
        if not np.array_equal(decoded,source[begin:begin+batch]):raise ValueError('coefficient bits changed')
        h.update(memoryview(decoded).cast('B'))
        if check:check('catalogue_stream_verification',8*min(batch,len(values)-begin))
    restored=h.hexdigest()
    if storage_sha(values)!=original or restored!=original:raise ValueError('original/decoded coefficient hash mismatch')
    catalogue.flags.writeable=False;codes.flags.writeable=False;encoded=catalogue.nbytes+codes.nbytes
    return catalogue,codes,dict(coefficients=len(values),catalogue_words=len(catalogue),original_bytes=values.nbytes,catalogue_bytes=catalogue.nbytes,index_bytes=codes.nbytes,encoded_bytes=encoded,saved_bytes=values.nbytes-encoded,batches=batches,coefficient_batch_cap=batch,maximum_batch_unique_words=maximum_unique,original_value_sha256=original,decoded_value_sha256=restored,encoded_array_sha256=storage_sha(catalogue,codes),bitwise_roundtrip_verified=True,original_values_preserved_bitwise=True,physical_precision='float64 bits unchanged; uint64 catalogue and uint32 lookup IDs',storage_potential_accepted=values.nbytes-encoded>=32*2**20)

def decode_chunks(catalogue,codes,batch=262144):
    if not isinstance(catalogue,np.ndarray) or catalogue.dtype!=np.dtype('uint64') or catalogue.ndim!=1 or not len(catalogue) or not catalogue.flags.c_contiguous or not isinstance(codes,np.ndarray) or codes.dtype!=np.dtype('uint32') or codes.ndim!=1 or not len(codes) or not codes.flags.c_contiguous or not isinstance(batch,int) or not 1<=batch<=262144:raise ValueError('invalid coefficient catalogue')
    for start in range(0,len(codes),batch):
        ids=codes[start:start+batch]
        if int(ids.max())>=len(catalogue):raise ValueError('catalogue ID out of bounds')
        yield catalogue[ids].view(np.float64)
