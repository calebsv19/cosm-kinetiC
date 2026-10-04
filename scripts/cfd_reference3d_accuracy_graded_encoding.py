"""Shared Float32 PC storage plus int32 bits, losslessly representing Float64 physics."""
import hashlib
import numpy as np
from cfd_reference3d_shared_factor import storage_sha

def encode(values,batch=262144,check=None):
    if not isinstance(values,np.ndarray) or values.ndim!=1 or values.dtype!=np.dtype('float64') or not values.flags.c_contiguous or not 1<=len(values)<=120000000 or not isinstance(batch,int) or not 1<=batch<=262144:raise ValueError('invalid exact predictor input')
    original=storage_sha(values)
    if check:check('predictor_output_allocation',8*len(values)+64*batch)
    predictor=np.empty(len(values),dtype=np.float32);correction=np.empty(len(values),dtype=np.int32);lower=0;upper=0;batches=0
    for begin in range(0,len(values),batch):
        if check:check('predictor_batch',64*min(batch,len(values)-begin))
        chunk=values[begin:begin+batch]
        if not np.all(np.isfinite(chunk)):raise ValueError('nonfinite physical coefficient')
        with np.errstate(over='ignore',under='ignore'):p=chunk.astype(np.float32)
        if not np.all(np.isfinite(p)) or np.any((chunk!=0)&(p==0)):raise ValueError('Float predictor overflow/underflow')
        base=p.astype(np.float64).view(np.uint64);delta=(chunk.view(np.uint64)-base).view(np.int64);lo=int(delta.min());hi=int(delta.max())
        if lo<np.iinfo(np.int32).min or hi>np.iinfo(np.int32).max:raise ValueError('exact word correction outside int32')
        predictor[begin:begin+len(chunk)]=p;correction[begin:begin+len(chunk)]=delta;lower=min(lower,lo);upper=max(upper,hi);batches+=1
        del p,base,delta,chunk
        if check:check('predictor_batch_complete',0)
    h=hashlib.sha256();h.update(str((values.shape,values.dtype.str)).encode())
    for begin in range(0,len(values),batch):
        bits=predictor[begin:begin+batch].astype(np.float64).view(np.uint64)+correction[begin:begin+batch].astype(np.int64).view(np.uint64)
        if not np.array_equal(bits,values[begin:begin+batch].view(np.uint64)):raise ValueError('exact predictor changed physical bits')
        h.update(memoryview(bits).cast('B'))
        if check:check('predictor_stream_verification',32*min(batch,len(values)-begin))
    decoded=h.hexdigest()
    if storage_sha(values)!=original or decoded!=original:raise ValueError('original/decoded physical hash mismatch')
    predictor.flags.writeable=False;correction.flags.writeable=False;encoded=predictor.nbytes+correction.nbytes;original_joint=values.nbytes+predictor.nbytes;saved=original_joint-encoded
    return predictor,correction,dict(coefficients=len(values),physical_double_bytes=values.nbytes,original_pc_float_bytes=predictor.nbytes,original_joint_bytes=original_joint,predictor_bytes=predictor.nbytes,word_correction_bytes=correction.nbytes,encoded_bytes=encoded,encoded_joint_bytes=encoded,saved_joint_bytes=saved,batches=batches,coefficient_batch_cap=batch,minimum_word_correction=lower,maximum_word_correction=upper,original_value_sha256=original,decoded_value_sha256=decoded,predictor_float_sha256=storage_sha(predictor),encoded_array_sha256=storage_sha(predictor,correction),bitwise_roundtrip_verified=True,original_values_preserved_bitwise=True,physical_precision='original float64 bits restored by exact integer word correction; float32 predictor shared with PC only',storage_potential_accepted=saved>=32*2**20)

def decode_chunks(predictor,correction,batch=262144):
    if not isinstance(predictor,np.ndarray) or predictor.dtype!=np.dtype('float32') or predictor.ndim!=1 or not len(predictor) or not predictor.flags.c_contiguous or not isinstance(correction,np.ndarray) or correction.dtype!=np.dtype('int32') or correction.shape!=predictor.shape or not correction.flags.c_contiguous or not isinstance(batch,int) or not 1<=batch<=262144:raise ValueError('invalid exact predictor')
    for begin in range(0,len(predictor),batch):
        bits=predictor[begin:begin+batch].astype(np.float64).view(np.uint64)+correction[begin:begin+batch].astype(np.int64).view(np.uint64)
        yield bits.view(np.float64)
