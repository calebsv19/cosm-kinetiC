"""Scientific value fidelity and bounded malformed-packet refusal, no solver calls."""
import array,copy,gc,json,math,struct,sys,types,unittest,zlib
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import field_packet as fp
from coupling.journal import Journal,FIELD_PACKET
from surface_sources.growth_fire_v1 import canonical
class FieldPacketTests(unittest.TestCase):
    def value(self):
        v=[-0.,0.,1e-300,-1e-300,.1,1e200]*50
        return {'a':v,'duplicate':list(v),'ints':[9007199254740993,-9007199254740993]*150,'bools':[True,False],'scalar':-0.,'name':'field μ','none':None}
    def mutate(self,blob,fn):
        _,n,p=fp.HEADER.unpack_from(blob);m=json.loads(blob[24:24+n]);fn(m);raw=json.dumps(m,separators=(',',':')).encode();return fp.HEADER.pack(fp.MAGIC,len(raw),p)+raw+blob[24+n:]
    def test_exact_bits_types_and_independent_result_lists(self):
        a=self.value();blob=fp.encode(a);b=fp.decode(blob);self.assertEqual(b,a)
        self.assertEqual(array.array('d',b['a']).tobytes(),array.array('d',a['a']).tobytes());self.assertTrue(math.copysign(1,b['scalar'])<0)
        self.assertIs(type(b['ints'][0]),int);self.assertIs(type(b['bools'][0]),bool);self.assertIsNot(b['a'],b['duplicate']);self.assertEqual(fp.encode(b),blob)
        _,n,_=fp.HEADER.unpack_from(blob);self.assertEqual(len(json.loads(blob[24:24+n])['arrays']),2)
    def test_successful_visitors_release_without_cyclic_collection(self):
        gc.collect();enabled=gc.isenabled();gc.disable()
        try:
            for _ in range(4):
                blob=fp.encode(self.value());decoded=fp.decode(blob)
                del decoded,blob
            visitors=[v for v in gc.get_objects() if isinstance(v,types.FunctionType)
                      and v.__name__=='visit' and v.__code__.co_filename==fp.__file__]
            self.assertEqual(visitors,[])
        finally:
            if enabled:gc.enable()
            gc.collect()
    def test_legacy_json_and_f64_journal_unchanged(self):
        value={'a':[float(i)/7 for i in range(300)],'signed':-0.}
        from coupling.journal_f64 import pack
        self.assertEqual(Journal().encode(value),canonical(value).decode())
        self.assertEqual(Journal('lossless_f64_zlib_1g/v1').encode(value),zlib.compress(canonical(pack(value)),6))
    def test_new_journal_never_expands_full_json(self):
        a=self.value();j=Journal(FIELD_PACKET)
        with patch('coupling.journal.canonical',side_effect=AssertionError('expandedJSON')):b=j.decode(j.encode(a))
        self.assertEqual(fp.encode(a),fp.encode(b));self.assertEqual(j.limit,1024**3)
        with self.assertRaises(ValueError):j.decode(j.encode(a)+b'x')
    def test_truncation_trailing_crc_and_magic_refuse(self):
        b=fp.encode(self.value())
        for bad in [b[:20],b[:-1],b+b'x',b'BADMAGIC'+b[8:],b[:-1]+bytes([b[-1]^1])]:
            with self.subTest(length=len(bad)),self.assertRaises(ValueError):fp.decode(bad)
    def test_descriptor_type_offset_count_and_reference_refuse(self):
        b=fp.encode(self.value())
        for fn in [lambda m:m['arrays'][0].update(dtype='f32'),lambda m:m['arrays'][0].update(count=fp.MAX_COUNT+1),lambda m:m['arrays'][0].update(offset=1),lambda m:m['value']['a'].update({fp.TAG:True}),lambda m:m['value']['a'].update({fp.TAG:999}),lambda m:m['value']['a'].update(extra=1)]:
            with self.subTest(fn=fn),self.assertRaises(ValueError):fp.decode(self.mutate(b,fn))
    def test_reserved_nonfinite_depth_and_tight_budget(self):
        for value in [{fp.TAG:0},[float('nan')]*256,{'a':float('inf')},[None]*0+[{fp.TAG:None}]]:
            with self.assertRaises(ValueError):fp.encode(value)
        x={}
        for _ in range(34):x={'nested':x}
        with self.assertRaises(ValueError):fp.encode(x)
        with self.assertRaises(ValueError):fp.encode(self.value(),limit=64)
    def test_nonfinite_payload_with_valid_crc_refuses(self):
        b=fp.encode({'a':[0.]*256});_,n,p=fp.HEADER.unpack_from(b);m=json.loads(b[24:24+n]);raw=struct.pack('<Q',0x7ff8000000000000)+b[24+n+8:];m['arrays'][0]['crc32']=zlib.crc32(raw);meta=json.dumps(m,separators=(',',':')).encode();bad=fp.HEADER.pack(fp.MAGIC,len(meta),p)+meta+raw
        with self.assertRaises(ValueError):fp.decode(bad)
    def test_alias_bomb_and_unreferenced_data_refuse(self):
        b=fp.encode({'a':[1.]*256});bad=self.mutate(b,lambda m:m.update(value={}))
        with self.assertRaises(ValueError):fp.decode(bad)
        with patch.object(fp,'MAX_LOGICAL',2048):
            with self.assertRaises(ValueError):fp.decode(self.mutate(b,lambda m:m['value'].update(b={fp.TAG:0})))
if __name__=='__main__':unittest.main()
