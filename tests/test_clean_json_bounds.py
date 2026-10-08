"""Cleanup JSON resource boundaries are enforced before unsafe decoding."""
from pathlib import Path
import sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from check_clean_root import read_json

class CleanJsonBounds(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.path=Path(self.t.name)/'metadata.json'
    def read(self,text):
        self.path.write_text(text);return read_json(self.path,1024*1024)
    def test_overflowing_number_is_held_like_nonfinite_constants(self):
        for value in ('1e999','-1e999','NaN','Infinity','-Infinity'):
            with self.subTest(value=value),self.assertRaisesRegex(ValueError,'[Nn]on.?finite'):
                self.read('{"value":'+value+'}')
    def test_depth_limit_accepts_boundary_and_holds_one_more_container(self):
        self.assertIsInstance(self.read('{"value":'+'['*63+'0'+']'*63+'}'),dict)
        with self.assertRaisesRegex(ValueError,'nesting'):
            self.read('{"value":'+'['*64+'0'+']'*64+'}')
    def test_event_limit_accepts_boundary_and_holds_one_more_event(self):
        self.assertEqual(len(self.read('['+'0,'*99999+'0]')),100000)
        with self.assertRaisesRegex(ValueError,'structural'):
            self.read('['+'0,'*100000+'0]')
    def test_string_punctuation_and_escapes_do_not_spend_structure_budget(self):
        import json
        value=('[]{}:,"\\'*20000)
        self.assertEqual(self.read(json.dumps({'text':value,'finite':1e100})),{'text':value,'finite':1e100})
    def test_numeric_token_length_accepts_boundary_and_holds_one_more_character(self):
        integer='1'+'0'*127;floating='0.'+'0'*126
        self.assertEqual(self.read(integer),10**127);self.assertEqual(self.read(floating),0.0)
        for token in (integer+'0',floating+'0'):
            with self.subTest(token=token),self.assertRaisesRegex(ValueError,'numeric token'):
                self.read(token)

    def test_duplicate_fields_and_invalid_syntax_still_hold(self):
        for text in ('{"v":1,"v":2}','{} trailing','{"v":1,}', '[1,]'):
            with self.subTest(text=text),self.assertRaises(ValueError):self.read(text)

if __name__=='__main__':unittest.main()
