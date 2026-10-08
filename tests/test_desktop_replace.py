"""Desktop predecessor preservation and failure readback using tiny fake apps."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import signal
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import desktop_replace as replacement


class Replacement(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve()/'repo';self.repo.mkdir()
        self.home=Path(self.temp.name).resolve()/'home';(self.home/'Desktop').mkdir(parents=True)
        self.source=self.repo/'dist/kinetiC.app';self.destination=self.home/'Desktop/kinetiC.app'
        self.app(self.source,b'new');self.app(self.destination,b'old')
        self.environment=patch.dict(os.environ,HOME=str(self.home));self.environment.start();self.addCleanup(self.environment.stop)

    def app(self,path,content,bundle_id='com.cosm.kinetic'):
        (path/'Contents/MacOS').mkdir(parents=True)
        (path/'Contents/MacOS/binary').write_bytes(content)
        (path/'Contents/empty').mkdir()
        with (path/'Contents/Info.plist').open('wb') as stream:plistlib.dump({'CFBundleIdentifier':bundle_id},stream)
        (path/'Contents/alias').symlink_to('MacOS/binary')

    def run_replace(self):
        return replacement.replace(self.repo,self.source,self.destination,'com.cosm.kinetic')

    def receipts(self):
        return list((self.home/'Desktop/.physics-sim-app-history').glob('*/receipt.json'))

    def test_completed_copy_preserves_predecessor_and_repeat_history(self):
        old=replacement.inventory(self.destination);new=replacement.inventory(self.source)
        result=self.run_replace()
        self.assertEqual(replacement.inventory(Path(result['predecessor'])),old)
        self.assertEqual(replacement.inventory(self.destination),new)
        self.assertEqual(replacement.inventory(self.source),new)
        record=json.loads(Path(result['receipt']).read_text())
        self.assertEqual(record['state'],'completed');self.assertFalse(record['authentication_verified'])
        self.run_replace();self.assertEqual(len(self.receipts()),2)
        self.assertEqual(replacement.inventory(Path(result['predecessor'])),old)

    def test_failed_candidate_copy_leaves_installed_app_unchanged(self):
        old=replacement.inventory(self.destination)
        with patch.object(replacement,'copy_bundle',side_effect=OSError('copy failed')):
            with self.assertRaises(OSError):self.run_replace()
        self.assertEqual(replacement.inventory(self.destination),old)
        record=json.loads(self.receipts()[0].read_text())
        self.assertEqual(record['state'],'failed_retained');self.assertTrue(record['destination_restored_or_unchanged'])
        self.run_replace()

    def test_corrupt_predecessor_copy_refuses_replacement(self):
        old=replacement.inventory(self.destination)
        def copy(source,target):
            shutil.copytree(source,target,symlinks=True)
            if source==self.destination:(target/'Contents/MacOS/binary').write_bytes(b'corrupt')
        with patch.object(replacement,'copy_bundle',side_effect=copy):
            with self.assertRaisesRegex(ValueError,'Predecessor copy'):self.run_replace()
        self.assertEqual(replacement.inventory(self.destination),old)

    def test_publication_failure_restores_old_app_and_keeps_verified_backup(self):
        old=replacement.inventory(self.destination);rename=replacement.rename_exclusive
        def fail(source,target):
            if Path(source).name=='kinetiC.app' and Path(source).parent.name not in ('Desktop','displaced'):
                raise OSError('publication failed')
            return rename(source,target)
        with patch.object(replacement,'rename_exclusive',side_effect=fail):
            with self.assertRaises(OSError):self.run_replace()
        self.assertEqual(replacement.inventory(self.destination),old)
        receipt=self.receipts()[0];record=json.loads(receipt.read_text())
        self.assertEqual(record['last_state'],'predecessor_displaced')
        self.assertTrue(record['destination_restored_or_unchanged'])
        self.assertEqual(replacement.inventory(receipt.parent/'predecessor/kinetiC.app'),old)

    def test_identity_escape_and_special_file_holds_preserve_all_inputs(self):
        old=replacement.inventory(self.destination)
        with self.assertRaises(ValueError):replacement.replace(self.repo,self.source,self.home/'other.app','com.cosm.kinetic')
        with self.assertRaises(ValueError):replacement.replace(self.repo,self.source,self.destination,'wrong.id')
        link=self.source/'Contents/escape';link.symlink_to(self.home)
        with self.assertRaises(ValueError):self.run_replace()
        link.unlink();os.mkfifo(self.source/'Contents/pipe')
        with self.assertRaises(ValueError):self.run_replace()
        self.assertEqual(replacement.inventory(self.destination),old)
        self.assertEqual(self.receipts(),[])

    def test_unreadable_history_is_held_without_blocking(self):
        history=self.home/'Desktop/.physics-sim-app-history/broken';history.mkdir(parents=True)
        receipt=history/'receipt.json';os.mkfifo(receipt)
        with self.assertRaisesRegex(ValueError,'unreadable history'):self.run_replace()
        self.assertEqual((self.destination/'Contents/MacOS/binary').read_bytes(),b'old')

    def test_actual_refresh_recipe_uses_preservation_helper(self):
        scripts=self.repo/'scripts';scripts.mkdir()
        for name in ('desktop_replace.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        source=(ROOT/'make/package-macos.mk').read_text()
        target='package-desktop-refresh'
        section=source.split(target+':',1)[1].split('\n\n',1)[0]
        makefile=self.repo/'Makefile'
        makefile.write_text('PACKAGE_APP_DIR := '+str(self.source)+'\nDESKTOP_APP_DIR := '+str(self.destination)+'\nPACKAGE_BUNDLE_ID := com.cosm.kinetic\nPACKAGE_APP_NAME := kinetiC.app\npackage-desktop-refresh-authority package-desktop:\n\t@true\n'+target+':'+section+'\n')
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        result=subprocess.run(['make',target],cwd=self.repo,env=env,capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        receipt=json.loads(self.receipts()[0].read_text())
        self.assertEqual(receipt['state'],'completed')
        self.assertEqual((self.receipts()[0].parent/'predecessor/kinetiC.app/Contents/MacOS/binary').read_bytes(),b'old')

    def crash_replace(self, boundary):
        before=set(self.receipts())
        code = """
import os, signal, sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import desktop_replace as replacement
repo, source, destination = map(Path,sys.argv[2:5])
boundary=sys.argv[5]
rename=replacement.rename_exclusive
def crash(source,target):
    is_displace=Path(target).parent.name=='displaced'
    if is_displace and boundary=='before_displace':os.kill(os.getpid(),signal.SIGKILL)
    result=rename(source,target)
    if is_displace and boundary=='after_displace':os.kill(os.getpid(),signal.SIGKILL)
    if Path(target)==destination and boundary=='after_publish':os.kill(os.getpid(),signal.SIGKILL)
    return result
replacement.rename_exclusive=crash
replacement.replace(repo,source,destination,'com.cosm.kinetic')
"""
        process=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(self.repo),str(self.source),str(self.destination),boundary],capture_output=True,text=True,timeout=10)
        self.assertEqual(process.returncode,-signal.SIGKILL,process.stdout+process.stderr)
        created=set(self.receipts())-before
        self.assertEqual(len(created),1)
        return created.pop().parent

    def test_sigkill_boundaries_plan_recovery_and_preserve_attempt_journal(self):
        # Each child is killed at an actual filesystem transition, not simulated
        # by raising an exception that would run the helper's rollback handler.
        for boundary,action in (('before_displace','reconcile_predecessor'),('after_displace','restore_predecessor'),('after_publish','reconcile_candidate')):
            with self.subTest(boundary=boundary):
                if self.destination.exists():shutil.rmtree(self.destination)
                self.app(self.destination,b'old')
                # Keep completed recovery records from earlier subcases. Their
                # admission remains terminal even after later successful refresh.
                attempt=self.crash_replace(boundary)
                receipt=attempt/'receipt.json';original=receipt.read_bytes()
                plan=replacement.recover(attempt)
                self.assertEqual(plan['state'],'plan_only');self.assertEqual(plan['action'],action)
                self.assertFalse((attempt/'recovery').exists())
                result=replacement.recover(attempt,apply=True)
                self.assertEqual(result['state'],'completed')
                expected=b'new' if boundary=='after_publish' else b'old'
                self.assertEqual((self.destination/'Contents/MacOS/binary').read_bytes(),expected)
                self.assertEqual(receipt.read_bytes(),original)
                again=replacement.recover(attempt,apply=True)
                self.assertEqual(again['state'],'already_recovered')
                self.run_replace();self.run_replace()

    def test_recovery_refuses_tampered_backup_and_occupied_destination(self):
        attempt=self.crash_replace('after_displace')
        backup=attempt/'predecessor/kinetiC.app/Contents/MacOS/binary'
        backup.write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'predecessor inventory mismatch'):replacement.recover(attempt,apply=True)
        self.assertFalse(self.destination.exists())
        backup.write_bytes(b'old')
        self.app(self.destination,b'unrelated')
        with self.assertRaisesRegex(ValueError,'occupied destination'):replacement.recover(attempt,apply=True)
        self.assertEqual((self.destination/'Contents/MacOS/binary').read_bytes(),b'unrelated')

    def test_recovery_copy_failure_retains_backups_and_allows_verified_retry(self):
        attempt=self.crash_replace('after_displace')
        with patch.object(replacement,'copy_bundle',side_effect=OSError('recovery copy failed')):
            with self.assertRaises(OSError):replacement.recover(attempt,apply=True)
        self.assertFalse(self.destination.exists())
        self.assertEqual((attempt/'displaced/kinetiC.app/Contents/MacOS/binary').read_bytes(),b'old')
        self.assertEqual(json.loads(next((attempt/'recovery').glob('*/receipt.json')).read_text())['state'],'failed_retained')
        self.assertEqual(replacement.recover(attempt,apply=True)['state'],'completed')

    def test_recovery_lock_and_exact_attempt_path_are_enforced(self):
        attempt=self.crash_replace('after_displace')
        with self.assertRaises(ValueError):replacement.recover(self.repo,apply=True)
        lock=attempt.parent/(hashlib.sha256(str(self.destination).encode()).hexdigest()+'.lock')
        with lock.open('r') as stream:
            fcntl.flock(stream,fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError,'active Desktop'):replacement.recover(attempt,apply=True)
        self.assertFalse(self.destination.exists())

    def test_recovery_cli_defaults_to_plan_and_applies_exact_retained_attempt(self):
        attempt=self.crash_replace('after_displace')
        command=[sys.executable,'-B',str(ROOT/'scripts/desktop_replace.py'),'--recover-attempt',str(attempt)]
        planned=subprocess.run(command,capture_output=True,text=True,timeout=10)
        self.assertEqual(planned.returncode,0,planned.stderr)
        self.assertEqual(json.loads(planned.stdout)['state'],'plan_only')
        self.assertFalse(self.destination.exists());self.assertFalse((attempt/'recovery').exists())
        applied=subprocess.run(command+['--apply'],capture_output=True,text=True,timeout=10)
        self.assertEqual(applied.returncode,0,applied.stderr)
        self.assertEqual(json.loads(applied.stdout)['state'],'completed')
        self.assertEqual((self.destination/'Contents/MacOS/binary').read_bytes(),b'old')

    def test_exclusive_publication_refuses_even_an_empty_directory(self):
        source=self.repo/'candidate';source.mkdir();(source/'sentinel').write_bytes(b'candidate')
        target=self.repo/'occupied';target.mkdir()
        with self.assertRaises(FileExistsError):replacement.rename_exclusive(source,target)
        self.assertEqual(list(target.iterdir()),[])
        self.assertEqual((source/'sentinel').read_bytes(),b'candidate')

    def test_active_refresh_and_unfinished_attempt_hold(self):
        history=self.home/'Desktop/.physics-sim-app-history';history.mkdir()
        lock=history/(hashlib.sha256(str(self.destination).encode()).hexdigest()+'.lock')
        with lock.open('w') as stream:
            fcntl.flock(stream,fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError,'active refresh'):self.run_replace()
        previous=history/'interrupted';previous.mkdir()
        (previous/'receipt.json').write_text(json.dumps({'destination':str(self.destination),'state':'predecessor_displaced'}))
        with self.assertRaisesRegex(ValueError,'requires recovery'):self.run_replace()
        self.assertEqual((self.destination/'Contents/MacOS/binary').read_bytes(),b'old')


if __name__=='__main__':unittest.main()
