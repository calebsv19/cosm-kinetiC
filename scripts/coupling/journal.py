"""Opt-in lossless SQLite checkpoint codec. No scientific value is quantized.

Old TEXT journals keep their64MiB cap. Explicit zlib journals admit512MiB on
disk. The separately selected lossless_f64_zlib_1g/v1 capacity admits1GiB
without changing payload encoding. Decoded records remain capped256MiB;
history stays compressed until each record is requested.
"""
import json
import zlib
from surface_sources.growth_fire_v1 import canonical,require
FIELD_PACKET='lossless_field_packet_zlib_1g/v1'
FIELD_PACKET_2G='lossless_field_packet_zlib_2g/v1'
MAX_DECODED=256*1024*1024
class Journal:
    def __init__(self,mode=None):
        require(mode in (None,'lossless_zlib/v1','lossless_f64_zlib/v1','lossless_f64_zlib_1g/v1',FIELD_PACKET,FIELD_PACKET_2G),'journal codec')
        self.mode=mode
        self.compressed=mode is not None
        self.limit=(2048 if mode==FIELD_PACKET_2G else 1024 if mode in ('lossless_f64_zlib_1g/v1',FIELD_PACKET,FIELD_PACKET_2G) else 512 if self.compressed else 64)*1024*1024
    def encode(self,value):
        if self.mode in (FIELD_PACKET,FIELD_PACKET_2G):
            from field_packet import encode
            return zlib.compress(encode(value,MAX_DECODED),6)
        raw=canonical(value)
        require(len(raw)<=MAX_DECODED,'decoded checkpoint byte bound')
        if self.mode in ('lossless_f64_zlib/v1','lossless_f64_zlib_1g/v1'):
            from .journal_f64 import pack
            return zlib.compress(canonical(pack(value)),6)
        return zlib.compress(raw,3) if self.compressed else raw.decode()
    def decode(self,payload):
        if not self.compressed:
            require(type(payload) is str,'legacy journal TEXT payload');return json.loads(payload)
        require(type(payload) is bytes and len(payload)<=self.limit,'compressed journal BLOB payload')
        decoder=zlib.decompressobj();raw=decoder.decompress(payload,MAX_DECODED+1)
        require(len(raw)<=MAX_DECODED and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,'bounded complete compressed checkpoint')
        if self.mode in (FIELD_PACKET,FIELD_PACKET_2G):
            from field_packet import decode
            return decode(raw,MAX_DECODED)
        value=json.loads(raw)
        if self.mode in ('lossless_f64_zlib/v1','lossless_f64_zlib_1g/v1'):
            from .journal_f64 import unpack
            value=unpack(value)
            require(len(canonical(value))<=MAX_DECODED,'unpacked checkpoint byte bound')
        return value
    def rows(self,db):
        # Store compressed bytes only; do not inflate all historical grids at once.
        payloads=[v[0] for v in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]
        journal=self
        class Rows:
            def __len__(self):return len(payloads)
            def __getitem__(self,index):
                if isinstance(index,slice):return (journal.decode(v) for v in payloads[index])
                return journal.decode(payloads[index])
            def __iter__(self):return (journal.decode(v) for v in payloads)
        return Rows()
