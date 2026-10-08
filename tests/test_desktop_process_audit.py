"""A malformed or minified running process receipt cannot allow app replacement."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from check_desktop_process_audit import validate


class Audit(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();self.receipt=self.repo/'audit.json'
        self.destination=self.repo/'Desktop/kinetiC Main Edit.app';self.match='kinetiC Main Edit'
        self.row={'schema_version':'codework_mew1_process_audit_v1','match':self.match,'path':str(self.destination),'method':'lsof_path_fallback','running':False,'matches':[],'truncated':False}

    def check(self, row=None):
        self.receipt.write_text(json.dumps(self.row if row is None else row,separators=(',',':')))
        return validate(self.repo,self.receipt,self.match,self.destination)

    def test_complete_fresh_absence_accepts_minified_json_without_mutation(self):
        result=self.check();before=self.receipt.read_bytes()
        self.assertEqual(result['status'],'verified_absence')
        self.assertEqual(self.receipt.read_bytes(),before)

    def test_minified_running_true_would_miss_old_grep_and_is_held(self):
        row=dict(self.row,running=True,matches=[{'pid':12,'executable':'app'}],method='ps_literal')
        payload=json.dumps(row,separators=(',',':'))
        grep=subprocess.run(['grep','-Fq','"running": true'],input=payload,capture_output=True,text=True)
        self.assertEqual(grep.returncode,1)
        with self.assertRaisesRegex(ValueError,'running or matched'):self.check(row)

    def test_invalid_incomplete_truncated_and_wrong_scope_hold(self):
        variations=[dict(self.row,running='false'),dict(self.row,truncated=True),dict(self.row,matches={}),dict(self.row,matches=[{'pid':1}]),dict(self.row,method='ps_literal'),dict(self.row,path='/other/app'),dict(self.row,match='other'),dict(self.row,schema_version='other'),dict(self.row,error='lookup failed')]
        missing=dict(self.row);missing.pop('running');variations.append(missing)
        for row in variations:
            with self.subTest(row=row),self.assertRaises(ValueError):self.check(row)

    def test_stale_future_ambiguous_and_nonregular_receipts_hold(self):
        self.check()
        for timestamp in (time.time()-60,time.time()+60):
            os.utime(self.receipt,(timestamp,timestamp))
            with self.assertRaisesRegex(ValueError,'timestamp|stale'):validate(self.repo,self.receipt,self.match,self.destination)
        for payload in ('[]','{','{"running":true,"running":false}','{"running":NaN}'):
            self.receipt.write_text(payload)
            with self.assertRaises(ValueError):validate(self.repo,self.receipt,self.match,self.destination)
        self.receipt.unlink();os.mkfifo(self.receipt)
        with self.assertRaises(ValueError):validate(self.repo,self.receipt,self.match,self.destination)
        self.receipt.unlink();self.receipt.symlink_to(self.repo/'missing')
        with self.assertRaises(ValueError):validate(self.repo,self.receipt,self.match,self.destination)

    def test_actual_refresh_recipe_never_reaches_replacement_with_running_receipt(self):
        scripts=self.repo/'scripts';scripts.mkdir()
        for name in ('check_desktop_process_audit.py','check_clean_root.py','build_outputs.py','clean_outputs.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        marker=self.repo/'replacement-reached'
        (scripts/'desktop_replace.py').write_text("from pathlib import Path\nPath("+repr(str(marker))+").touch()\n")
        tool=self.repo/'mew1.py';proof=self.repo/'build/proof/work';proof.mkdir(parents=True)
        source=(ROOT/'make/package-macos.mk').read_text()
        recipe='_package-main-edit-refresh:'+source.split('_package-main-edit-refresh:',1)[1].split('main-edit-package-contract-checks:',1)[0]
        variables={'PACKAGE_PROOF_DIR':str(proof),'MAIN_EDIT_PROCESS_RECEIPT':str(proof/'audit.json'),'MEW1_TOOL':str(tool),'MAIN_EDIT_DISPLAY_NAME':self.match,'MAIN_EDIT_DESKTOP_APP_DIR':str(self.destination),'DESKTOP_APP_DIR':str(self.repo/'Desktop/kinetiC.app'),'MAIN_EDIT_APP_DIR':str(self.repo/'dist/kinetiC Main Edit.app'),'MAIN_EDIT_BUNDLE_ID':'com.cosm.kinetic.main-edit','MAIN_EDIT_APP_NAME':'kinetiC Main Edit.app'}
        (self.repo/'makefile').write_text('\n'.join(k+' := '+v for k,v in variables.items())+'\n'+recipe)
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        for running in (True,False):
            row=dict(self.row,running=running,matches=[{'pid':1,'executable':'app'}] if running else [],method='ps_literal' if running else 'lsof_path_fallback')
            tool.write_text('print('+repr(json.dumps(row,separators=(',',':')))+')\n')
            result=subprocess.run(['make','_package-main-edit-refresh'],cwd=self.repo,env=env,capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,2 if running else 0,result.stdout+result.stderr)
            self.assertEqual(marker.exists(),not running)


if __name__=='__main__':unittest.main()
