"""Memory-bounded implementation of the existing tagged-binary64 JSON digest.

It hashes identical bytes to growth_fire_v1.digest; there is no new wire format.
"""
import hashlib,json,struct
from surface_sources.growth_fire_v1 import number,require

def digest(value):
    require(type(value) is dict,'digest object')
    hasher=hashlib.sha256();buffer=bytearray()
    def emit(part):
        buffer.extend(part)
        if len(buffer)>=65536:hasher.update(buffer);buffer.clear()
    def visit(v):
        t=type(v)
        if t in (int,float):
            number(v);require(t is not int or int(float(v))==v,'integer not exactly representable as binary64')
            emit(b'{"$f64be":"'+struct.pack('>d',float(v)).hex().encode('ascii')+b'"}')
        elif t is dict:
            emit(b'{')
            for i,k in enumerate(sorted(v)):
                if i:emit(b',')
                emit(json.dumps(k,ensure_ascii=True).encode('ascii'));emit(b':');visit(v[k])
            emit(b'}')
        elif t is list:
            emit(b'[')
            for i,item in enumerate(v):
                if i:emit(b',')
                visit(item)
            emit(b']')
        else:emit(json.dumps(v,ensure_ascii=True,allow_nan=False,separators=(',',':')).encode('ascii'))
    visit({k:v for k,v in value.items() if k!='digest'});hasher.update(buffer);return hasher.hexdigest()
def sealed(value):return {**value,'digest':digest(value)}

def strict_load(path,limit_bytes):
    from pathlib import Path
    p=Path(path);require(p.stat().st_size<=limit_bytes,'native artifact byte bound')
    def pairs(items):
        out={}
        for key,value in items:require(key not in out,'duplicate JSON key');out[key]=value
        return out
    def constant(value):raise ValueError('nonfinite JSON constant: '+value)
    with p.open('rb') as file:raw=file.read(limit_bytes+1)
    require(len(raw)<=limit_bytes,'native artifact byte bound')
    try:return json.loads(raw,object_pairs_hook=pairs,parse_constant=constant)
    except RecursionError as error:raise ValueError('JSON nesting exceeds parser limit') from error
