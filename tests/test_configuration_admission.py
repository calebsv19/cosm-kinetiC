"""Configuration selection refuses unknown, malformed and interrupted metadata."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_outputs import record
from build_identity import active_selection, revalidate


class ConfigurationAdmission(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'scripts').mkdir()
        for name in ('tool_probe.py','build_identity.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py','build_owner.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        self.root=self.repo/'build/profile';self.active=self.root/'.configuration/active.json'
        self.env={key:value for key,value in os.environ.items() if not key.startswith('PHYSICS_BUILD_')}

    def cli(self,*args):
        return subprocess.run([sys.executable,'-B','scripts/build_identity.py','--selection-root','build/profile',*args],
            cwd=self.repo,env=self.env,text=True,capture_output=True,timeout=20)

    def selection(self):
        result=self.cli('--digest-only');self.assertEqual(result.returncode,0,result.stderr)
        return result.stdout.strip()

    def create(self):
        selection=self.selection();output=self.root/'.configuration'/(selection+'.json')
        result=self.cli('--output',str(output),'--expected-digest',selection)
        self.assertEqual(result.returncode,0,result.stderr)
        return output

    def test_unknown_active_is_held_before_compiler_probe(self):
        self.active.parent.mkdir(parents=True)
        self.active.write_text(json.dumps({'digest':'0'*64,'generation':1}))
        compiler=self.repo/'compiler';compiler.write_text('#!/bin/sh\ntouch invoked\n');compiler.chmod(0o755)
        self.env['PHYSICS_BUILD_CC']=str(compiler)
        result=self.cli('--digest-only')
        self.assertNotEqual(result.returncode,0)
        self.assertFalse((self.repo/'invoked').exists())
        self.assertEqual(list(self.active.parent.iterdir()),[self.active])

    def test_strict_registered_selection_and_special_files_are_held(self):
        self.active.parent.mkdir(parents=True)
        for content in ('{"digest":"'+ '0'*64 +'","generation":true}',
                        '{"digest":"'+ '0'*64 +'","generation":1,"generation":2}',
                        '{"digest":"bad","generation":1}'):
            self.active.write_text(content);record(self.repo,self.active,'configuration')
            with self.assertRaises(ValueError):active_selection(self.repo,self.active)
        self.active.unlink();os.mkfifo(self.active)
        with self.assertRaises(ValueError):active_selection(self.repo,self.active)

    def test_exact_stamp_path_and_matching_noop(self):
        output=self.create();before=(output.read_bytes(),output.stat().st_mtime_ns,self.active.stat().st_mtime_ns)
        result=self.cli('--output',str(output));self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((output.read_bytes(),output.stat().st_mtime_ns,self.active.stat().st_mtime_ns),before)
        unrelated=self.repo/'source.c';unrelated.write_bytes(b'preserved')
        result=self.cli('--output',str(unrelated));self.assertNotEqual(result.returncode,0)
        self.assertEqual(unrelated.read_bytes(),b'preserved')

    def test_changed_stamp_and_missing_active_are_held(self):
        output=self.create();original=output.read_bytes();output.write_bytes(b'changed metadata')
        result=self.cli('--output',str(output));self.assertNotEqual(result.returncode,0)
        self.assertEqual(output.read_bytes(),b'changed metadata')
        self.assertNotEqual(self.cli('--digest-only').returncode,0)
        output.write_bytes(original);record(self.repo,output,'configuration')
        self.active.unlink()
        result=self.cli('--digest-only');self.assertNotEqual(result.returncode,0)
        self.assertFalse(self.active.exists())
        self.assertIn('Missing active selection',result.stderr)

    def test_identity_change_during_selection_recheck_is_held(self):
        self.create();_,previous=active_selection(self.repo,self.active)
        self.active.write_bytes(self.active.read_bytes()+b' ')
        with self.assertRaises(ValueError):revalidate(self.repo,{self.active:previous})


if __name__=='__main__':unittest.main()
