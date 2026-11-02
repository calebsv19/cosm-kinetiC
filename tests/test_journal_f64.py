"""Binary journal packing must preserve exact scientific canonical bytes and digest."""
import sys,struct,unittest,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.journal_f64 import pack,unpack,TAG
from surface_sources.growth_fire_v1 import canonical,digest as reference
from numeric_digest_stream import digest
class BinaryJournal(unittest.TestCase):
    def test_exact_bits_and_digest(self):
        cases=[{'x':[0.,-0.,1e-300,-1e-300,1e300]*1000}, {'x':[float(i)/13 for i in range(20000)],'nested':{'v':[1,True,None,'x',2**53]}}]
        for value in cases:
            back=unpack(json.loads(canonical(pack(value))))
            self.assertEqual(canonical(value),canonical(back));self.assertEqual(digest(value),reference(value));self.assertEqual(digest(back),reference(value))
            for a,b in zip(value['x'],back['x']):self.assertEqual(struct.pack('>d',a),struct.pack('>d',b))
    def test_field_views_do_not_alias_checkpoint_arrays(self):
        names=('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','boundary_fluxes')
        value={'fluid':{'state':{'data':{k:[.5]*256 for k in names}},'fields':{k:[.5]*256 for k in names}}}
        back=unpack(pack(value));self.assertEqual(back,value)
        back['fluid']['fields']['energy_j'][0]=9
        self.assertEqual(back['fluid']['state']['data']['energy_j'][0],.5)
    def test_reserved_tags_and_wrong_counts(self):
        with self.assertRaises(ValueError):pack({TAG:'collision'})
        raw=pack({'x':[0.]*256});raw['x']['count']+=1
        with self.assertRaises(ValueError):unpack(raw)
    def test_nonfinite_and_integer_precision(self):
        for value in (float('nan'),float('inf'),-float('inf')):
            with self.assertRaises(ValueError):digest({'v':[value]*256})
        with self.assertRaises(ValueError):digest({'v':[2**53+1]*256})
        self.assertEqual(digest({'v':[2**53]*256}),reference({'v':[2**53]*256}))
if __name__=='__main__':unittest.main()
