"""Bounded lossless scientific field packets; CRC is not authentication.

Lifecycle SHA256 receipts retain provenance. JSON metadata/types and IEEE754
array bits survive unchanged. Repeated arrays are stored once by dtype+bytes.
"""
import array,hashlib,json,math,struct,sys,zlib
from surface_sources.growth_fire_v1 import require
FORMAT='field_packet/v1'
MAGIC=b'PSFPK01\n'
SCHEMA='physics_sim_field_packet/v1'
TAG='$field_packet_array_v1'
MAX_BYTES=256*1024**2
MAX_MANIFEST=1024**2
MAX_LOGICAL=512*1024**2
MAX_COUNT=8*1024**2
HEADER=struct.Struct('<8sQQ')

def _pairs(pairs):
    result={}
    for k,v in pairs:
        require(k not in result,'duplicate packet metadata key');result[k]=v
    return result

def _finite(v):
    raise ValueError('nonfinite packet metadata')

def encode(value,limit=MAX_BYTES):
    require(type(limit) is int and 0<limit<=MAX_BYTES,'packet byte limit')
    records=[];chunks=[];memo={};offset=0;logical=0
    def visit(v,depth=0):
        nonlocal offset,logical
        require(depth<=32,'packet nesting bound')
        if type(v) is dict:
            require(TAG not in v and all(type(k) is str for k in v),'reserved packet tag or nonstring key')
            return {k:visit(x,depth+1) for k,x in v.items()}
        if type(v) is list:
            dtype='f64' if len(v)>=256 and all(type(x) is float for x in v) else 'i64' if len(v)>=256 and all(type(x) is int and -(1<<63)<=x<(1<<63) for x in v) else None
            if dtype:
                require(len(v)<=MAX_COUNT,'packet array count');logical+=len(v)*8;require(logical<=MAX_LOGICAL,'packet logical array bound')
                if dtype=='f64':require(all(math.isfinite(x) for x in v),'finite packet array')
                a=array.array('d' if dtype=='f64' else 'q',v);require(a.itemsize==8,'eight-byte packet type')
                if sys.byteorder!='little':a.byteswap()
                raw=a.tobytes();key=(dtype,len(v),hashlib.sha256(raw).digest())
                if key in memo:return {TAG:memo[key]}
                require(len(records)<1024 and offset+len(raw)<=limit,'packet payload bound')
                i=len(records);memo[key]=i;records.append(dict(dtype=dtype,count=len(v),offset=offset,crc32=zlib.crc32(raw)));chunks.append(raw);offset+=len(raw)
                return {TAG:i}
            return [visit(x,depth+1) for x in v]
        require(v is None or type(v) in (str,int,float,bool),'packet JSON value')
        require(type(v) is not float or math.isfinite(v),'finite packet scalar')
        return v
    try:reduced=visit(value)
    finally:visit=None  # Break the recursive closure cycle before releasing field buffers.
    meta=json.dumps(dict(schema=SCHEMA,value=reduced,arrays=records),allow_nan=False,ensure_ascii=True,separators=(',',':')).encode()
    require(len(meta)<=MAX_MANIFEST and HEADER.size+len(meta)+offset<=limit,'packet complete byte bound')
    return HEADER.pack(MAGIC,len(meta),offset)+meta+b''.join(chunks)

def decode(blob,limit=MAX_BYTES):
    require(type(blob) is bytes and HEADER.size<=len(blob)<=limit<=MAX_BYTES,'bounded packet bytes')
    magic,mlen,plen=HEADER.unpack_from(blob)
    require(magic==MAGIC and 0<mlen<=MAX_MANIFEST and HEADER.size+mlen+plen==len(blob),'complete packet framing')
    m=json.loads(blob[HEADER.size:HEADER.size+mlen],object_pairs_hook=_pairs,parse_constant=_finite)
    require(type(m) is dict and set(m)=={'schema','value','arrays'} and m['schema']==SCHEMA,'packet manifest schema')
    records=m['arrays'];require(type(records) is list and len(records)<=1024,'packet records')
    start=HEADER.size+mlen;end=0;values=[]
    for r in records:
        require(type(r) is dict and set(r)=={'dtype','count','offset','crc32'},'packet record keys')
        require(r['dtype'] in ('f64','i64') and type(r['count']) is int and 256<=r['count']<=MAX_COUNT,'packet record type/count')
        require(type(r['offset']) is int and r['offset']==end and type(r['crc32']) is int and 0<=r['crc32']<1<<32,'packet record offset/CRC')
        end+=8*r['count'];require(end<=plen,'packet record payload range')
        raw=blob[start+r['offset']:start+end];require(zlib.crc32(raw)==r['crc32'],'packet array CRC mismatch')
        a=array.array('d' if r['dtype']=='f64' else 'q');a.frombytes(raw)
        if sys.byteorder!='little':a.byteswap()
        v=a.tolist();require(r['dtype']!='f64' or all(math.isfinite(x) for x in v),'finite decoded packet array');values.append(v)
    require(end==plen,'packet payload coverage');logical=0;used=set()
    def visit(v,depth=0):
        nonlocal logical
        require(depth<=32,'packet nesting bound')
        if type(v) is dict:
            if TAG in v:
                i=v[TAG];require(set(v)=={TAG} and type(i) is int and 0<=i<len(values),'packet array reference')
                logical+=len(values[i])*8;require(logical<=MAX_LOGICAL,'packet logical array bound');used.add(i);return list(values[i])
            return {k:visit(x,depth+1) for k,x in v.items()}
        if type(v) is list:return [visit(x,depth+1) for x in v]
        require(type(v) is not float or math.isfinite(v),'finite decoded packet scalar');return v
    try:result=visit(m['value'])
    finally:visit=None  # Decoded arrays must not wait for cyclic garbage collection.
    require(used==set(range(len(values))),'no unreferenced packet payload');return result
