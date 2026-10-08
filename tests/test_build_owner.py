"""Whole Make ownership excludes cleanup and conflicting configuration selection."""
import fcntl
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_BUILD_OWNER') and not k.endswith('_FD')}

class Ownership(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve()
        for folder in ('scripts','make','build/a','data/experiments'): (self.repo/folder).mkdir(parents=True)
        for name in ('build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py','atomic_output.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        (self.repo/'build/a/object.o').write_bytes(b'preserved')
        subprocess.run([sys.executable,'-B','-c',"import sys;sys.path.insert(0,'scripts');from pathlib import Path;from build_outputs import record;record(Path.cwd(),Path('build/a/object.o').absolute(),'compiler')"],cwd=self.repo,check=True)

    def owner(self, root, *command):
        return [sys.executable,'-B','scripts/build_owner.py','--root',root,'--',*command]

    def wait_ready(self,process):
        deadline=time.monotonic()+5
        while not (self.repo/'ready').exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(.01)
        self.assertTrue((self.repo/'ready').exists())

    def clean_command(self):
        return [sys.executable,'-B','scripts/clean_outputs.py','--build-root','build/a','--test-root','tmp/tests','--experiment-root','data/experiments','--tools-root','data/tools','--apply']

    def test_live_build_blocks_same_root_and_cleanup_allows_independent_root(self):
        code="from pathlib import Path;from build_owner import inherited;import time;assert inherited(Path.cwd(),Path.cwd()/'build/a');Path('ready').touch();time.sleep(30)"
        process=subprocess.Popen(self.owner('build/a',sys.executable,'-c',"import sys;sys.path.insert(0,'scripts');"+code),cwd=self.repo,env=ENV)
        try:
            self.wait_ready(process)
            same=subprocess.run(self.owner('build/a',sys.executable,'-c',"raise Exception('must not execute')"),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(same.returncode,2);self.assertIn('held',same.stderr)
            other=subprocess.run(self.owner('build/b',sys.executable,'-c',"from pathlib import Path;Path('independent').touch()"),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(other.returncode,0,other.stderr)
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertNotEqual(clean.returncode,0)
            self.assertEqual((self.repo/'build/a/object.o').read_bytes(),b'preserved')
            process.send_signal(signal.SIGTERM);self.assertEqual(process.wait(timeout=5),143)
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(clean.returncode,0,clean.stderr);self.assertFalse((self.repo/'build/a').exists())
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=5)

    def test_overlapping_roots_conflict_in_both_orders_and_siblings_remain_independent(self):
        for active, blocked, independent in (
            ('build/a', 'build/a/nested', 'build/b'),
            ('build/a/nested', 'build/a', 'build/a/sibling'),
            ('build/a/nested', 'build', 'build/b'),
            ('build', 'build/a', None),
        ):
            with self.subTest(active=active, blocked=blocked):
                (self.repo/'ready').unlink(missing_ok=True)
                process=subprocess.Popen(self.owner(active,sys.executable,'-c',
                    "from pathlib import Path;import time;Path('ready').touch();time.sleep(30)"),cwd=self.repo,env=ENV)
                try:
                    self.wait_ready(process)
                    result=subprocess.run(self.owner(blocked,sys.executable,'-c',
                        "from pathlib import Path;Path('unexpected').touch()"),cwd=self.repo,env=ENV,capture_output=True,text=True)
                    self.assertEqual(result.returncode,2,result.stderr)
                    self.assertFalse((self.repo/'unexpected').exists())
                    if independent:
                        result=subprocess.run(self.owner(independent,sys.executable,'-c','pass'),cwd=self.repo,env=ENV,capture_output=True,text=True)
                        self.assertEqual(result.returncode,0,result.stderr)
                finally:
                    process.terminate();process.wait(timeout=5)
                result=subprocess.run(self.owner(blocked,sys.executable,'-c','pass'),cwd=self.repo,env=ENV,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)

    def test_escaped_compiler_retains_lock_after_supervisor_and_anchor_are_killed(self):
        script="import os,pathlib,sys,time;os.setsid();pathlib.Path('pids').write_text(str(os.getpid())+' '+str(os.getppid()));pathlib.Path('ready').touch();deadline=time.monotonic()+10\nwhile not pathlib.Path('release').exists() and time.monotonic()<deadline:time.sleep(.01)\npathlib.Path(sys.argv[-1]).write_bytes(b'new output')"
        command=[sys.executable,'-B','scripts/atomic_output.py','--',sys.executable,'-c',script,'-o','build/a/object.o']
        process=subprocess.Popen(self.owner('build/a',*command),cwd=self.repo,env=ENV)
        helper=None
        try:
            self.wait_ready(process)
            compiler,helper=map(int,(self.repo/'pids').read_text().split())
            process.kill();process.wait(timeout=5)
            os.kill(helper,signal.SIGKILL)
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertNotEqual(clean.returncode,0)
            # Inventory may reject the held stage before testing kernel locks.
            # The independent overlapping-root attempt below must still prove
            # that the escaped compiler retains the inherited owner descriptors.
            self.assertTrue('held by active build' in clean.stderr or 'Unknown build directory; cleanup held' in clean.stderr,clean.stderr)
            self.assertEqual((self.repo/'build/a/object.o').read_bytes(),b'preserved')
            parent=subprocess.run(self.owner('build',sys.executable,'-c','pass'),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(parent.returncode,2,parent.stderr)
            (self.repo/'release').touch()
            deadline=time.monotonic()+5
            while time.monotonic()<deadline:
                parent=subprocess.run(self.owner('build',sys.executable,'-c','pass'),cwd=self.repo,env=ENV,capture_output=True,text=True)
                if parent.returncode==0:break
                time.sleep(.01)
            self.assertEqual(parent.returncode,0,parent.stderr)
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertNotEqual(clean.returncode,0)
            self.assertIn('Unknown disposable output; cleanup held',clean.stderr)
            self.assertEqual((self.repo/'build/a/object.o').read_bytes(),b'preserved')
        finally:
            (self.repo/'release').touch()
            if process.poll() is None:process.kill();process.wait()

    def test_active_cleanup_blocks_build_before_any_output(self):
        locks=self.repo/'tmp/locks';locks.mkdir(parents=True)
        with (locks/'clean.lock').open('w') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            result=subprocess.run(self.owner('build/a',sys.executable,'-c',"from pathlib import Path;Path('unexpected').touch()"),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(result.returncode,2);self.assertFalse((self.repo/'unexpected').exists())

    def test_anchor_is_not_polled_before_group_cleanup(self):
        script="""import sys
sys.path.insert(0,'scripts')
import build_owner as b
class Child(b.subprocess.Popen):
    def poll(self):raise AssertionError('Build anchor polled before cleanup')
b.subprocess.Popen=Child
raise SystemExit(b.run('build/a',[sys.executable,'-c','pass']))
"""
        result=subprocess.run([sys.executable,'-B','-c',script],cwd=self.repo,env=ENV,
                              capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_interruption_preserves_nested_cleanup_grace(self):
        script="""import pathlib,signal,time,sys
def stop(*args):
    time.sleep(.15)
    pathlib.Path('nested-cleanup-complete').touch()
    sys.exit(0)
signal.signal(signal.SIGTERM,stop)
pathlib.Path('ready').touch()
time.sleep(30)
"""
        process=subprocess.Popen(self.owner('build/a',sys.executable,'-c',script),cwd=self.repo,
                                 env=ENV,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            self.wait_ready(process)
            process.send_signal(signal.SIGTERM)
            out,err=process.communicate(timeout=8)
            self.assertEqual(process.returncode,143,out+err)
            self.assertTrue((self.repo/'nested-cleanup-complete').exists())
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(clean.returncode,0,clean.stderr)
        finally:
            if process.poll() is None:process.kill();process.communicate(timeout=8)

    def test_parent_death_stops_in_group_writer(self):
        script="import pathlib,time;pathlib.Path('ready').touch();time.sleep(1.5);pathlib.Path('unexpected-parent-death').touch()"
        process=subprocess.Popen(self.owner('build/a',sys.executable,'-c',script),cwd=self.repo,
                                 env=ENV,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            self.wait_ready(process)
            process.kill();process.wait(timeout=5)
            time.sleep(1.7)
            self.assertFalse((self.repo/'unexpected-parent-death').exists())
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertEqual(clean.returncode,0,clean.stderr)
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=5)

    def test_completed_command_group_is_cleaned_before_owner_release(self):
        script="import subprocess,sys;subprocess.Popen([sys.executable,'-c',\"import pathlib,time;time.sleep(1);pathlib.Path('unexpected-lingering-writer').touch()\"])"
        result=subprocess.run(self.owner('build/a',sys.executable,'-c',script),cwd=self.repo,
                              env=ENV,capture_output=True,text=True,timeout=8)
        self.assertEqual(result.returncode,0,result.stderr)
        time.sleep(1.2)
        self.assertFalse((self.repo/'unexpected-lingering-writer').exists())
        clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertEqual(clean.returncode,0,clean.stderr)

    def test_reaped_anchor_never_authorizes_saved_group_signal(self):
        script="""import sys
from unittest.mock import patch
sys.path.insert(0,'scripts')
import build_owner as b
class Reaped:
    returncode=0
    pid=12345
with patch.object(b.os,'killpg',side_effect=AssertionError('saved group signaled')):
    try:b.signal_build_group(Reaped(),15)
    except ValueError:pass
    else:raise AssertionError('Reaped group accepted')
"""
        result=subprocess.run([sys.executable,'-B','-c',script],cwd=self.repo,env=ENV,
                              capture_output=True,text=True,timeout=8)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_term_ignoring_build_is_bounded_and_unknown_output_stays_held(self):
        script="import pathlib,signal,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);pathlib.Path('build/a/unknown-partial').write_bytes(b'partial');pathlib.Path('ready').touch();time.sleep(30)"
        process=subprocess.Popen(self.owner('build/a',sys.executable,'-c',script),cwd=self.repo,
                                 env=ENV,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            self.wait_ready(process)
            process.send_signal(signal.SIGTERM)
            out,err=process.communicate(timeout=10)
            self.assertEqual(process.returncode,2,out+err)
            self.assertIn('terminal ownership unverified',err)
            clean=subprocess.run(self.clean_command(),cwd=self.repo,env=ENV,capture_output=True,text=True)
            self.assertNotEqual(clean.returncode,0)
            self.assertIn('Unknown disposable output',clean.stderr)
            self.assertEqual((self.repo/'build/a/unknown-partial').read_bytes(),b'partial')
            self.assertEqual((self.repo/'build/a/object.o').read_bytes(),b'preserved')
        finally:
            if process.poll() is None:process.kill();process.communicate(timeout=8)

    def test_invalid_supervision_bound_is_refused_before_launch(self):
        script="""import sys
from unittest.mock import patch
sys.path.insert(0,'scripts')
import build_owner as b
with patch.object(b.subprocess,'Popen',side_effect=AssertionError('invalid bound launched')):
    for value in (0,-1,True,float('nan'),float('inf'),86401,'10'):
        try:b.supervise_build(['missing'],wall_cap=value)
        except ValueError:pass
        else:raise AssertionError('invalid bound accepted')
"""
        result=subprocess.run([sys.executable,'-B','-c',script],cwd=self.repo,env=ENV,
                              capture_output=True,text=True,timeout=8)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_actual_top_level_make_preserves_overrides_and_owns_complete_recipe(self):
        text=(ROOT/'makefile').read_text();(self.repo/'makefile').write_text(text)
        for name in re.findall(r'^include (make/[^\n]+)',text,re.M):
            (self.repo/name).write_text('')
        for name in ('config.mk','sources-tools.mk','rules-runtime.mk'):
            shutil.copy2(ROOT/'make'/name,self.repo/'make'/name)
        for name in ('VERSION','WORKER_VERSION'):(self.repo/name).write_text('test\n')
        (self.repo/'probe.py').write_text("from pathlib import Path\nimport sys,time\nsys.path.insert(0,'scripts')\nfrom build_owner import inherited\nassert inherited(Path.cwd(),Path.cwd()/'build/a')\nPath('ready').touch()\ntime.sleep(.3)\nPath('flags').write_text(sys.argv[1])\n")
        (self.repo/'make/rules-build.mk').write_text("all: first second\nfirst:\n\tpython3 -B probe.py '$(CFLAGS)'\nsecond: first\n\t@touch second\n")
        process=subprocess.Popen(['make','-j2','BUILD_DIR=build/a','CFLAGS=-DVALUE=2 -g','first','second'],cwd=self.repo,env=ENV,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.wait_ready(process)
        clean=subprocess.run(['make','BUILD_DIR=build/a','clean'],cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertNotEqual(clean.returncode,0)
        out,err=process.communicate(timeout=10);self.assertEqual(process.returncode,0,out+err)
        self.assertNotIn('jobserver unavailable',err)
        self.assertEqual((self.repo/'flags').read_text(),'-DVALUE=2 -g')
        self.assertTrue((self.repo/'second').exists())
        mixed=subprocess.run(['make','BUILD_DIR=build/a','clean','all'],cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertNotEqual(mixed.returncode,0)
        self.assertIn('separate operations',mixed.stderr)
        result=subprocess.run(['make','-n','BUILD_DIR=build/a','first'],cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr);self.assertNotIn('build_owner.py --root',result.stdout)

if __name__=='__main__':unittest.main()
