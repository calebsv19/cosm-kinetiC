"""Failure, interruption and dependency publication through actual compiler commands."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"scripts"))
from build_outputs import record


class AtomicOutput(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name)
        (self.repo/'scripts').mkdir()
        (self.repo/'build').mkdir()
        for name in ('atomic_output.py', 'build_owner.py', 'build_outputs.py', 'clean_outputs.py', 'check_clean_root.py'):
            shutil.copy2(ROOT/'scripts'/name, self.repo/'scripts'/name)

    def command(self, *argv):
        return [sys.executable, '-B', 'scripts/atomic_output.py', '--', *argv]

    def invoke(self, *argv):
        return subprocess.run(self.command(*argv), cwd=self.repo, capture_output=True, text=True, timeout=30)

    def test_compile_link_dependencies_and_failed_rebuild_preserve_last_good(self):
        (self.repo/'probe.c').write_text('int main(void){return 7;}\n')
        result = self.invoke('clang', '-MMD', '-MP', '-c', 'probe.c', '-o', 'build/probe.o')
        self.assertEqual(result.returncode, 0, result.stderr)
        obj = self.repo/'build/probe.o'; dep = obj.with_suffix('.d')
        self.assertTrue(dep.read_text().startswith('build/probe.o:'))
        self.assertNotIn('.staging-', dep.read_text())
        before = (obj.read_bytes(), dep.read_bytes(), obj.stat().st_mtime_ns)
        (self.repo/'probe.c').write_text('this is invalid C\n')
        result = self.invoke('clang', '-MMD', '-c', 'probe.c', '-o', 'build/probe.o')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((obj.read_bytes(), dep.read_bytes(), obj.stat().st_mtime_ns), before)
        result = self.invoke('clang', 'build/probe.o', '-o', 'build/probe')
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run([str(self.repo/'build/probe')])
        self.assertEqual(result.returncode, 7)
        self.assertEqual(list((self.repo/'build').glob('.*.staging-*')), [])

    def test_make_failed_rebuild_preserves_last_good_with_delete_on_error(self):
        (self.repo/'src').mkdir()
        source=self.repo/'src/probe.c';source.write_text('int value(void){return 7;}\n')
        lines=(ROOT/'make/rules-build.mk').read_text().splitlines()
        start=lines.index('$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c')
        rule='\n'.join(lines[start:start+3])+'\n'
        (self.repo/'Makefile').write_text('BUILD_DIR=build\nSRC_DIR=src\nCC=clang\nall: build/probe.o\n.DELETE_ON_ERROR:\n'+rule)
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        first=subprocess.run(['make'],cwd=self.repo,env=env,capture_output=True,text=True)
        self.assertEqual(first.returncode,0,first.stderr)
        obj=self.repo/'build/probe.o'; before=obj.read_bytes()
        source.write_text('invalid C;\n')
        # Force the source newer on hosts whose Make compares whole-second mtimes.
        newer=obj.stat().st_mtime+2;os.utime(source,(newer,newer))
        failed=subprocess.run(['make'],cwd=self.repo,env=env,capture_output=True,text=True)
        self.assertNotEqual(failed.returncode,0)
        self.assertEqual(obj.read_bytes(),before)

    def test_partial_output_failure_does_not_publish(self):
        output = self.repo/'build/probe'; output.write_bytes(b'last good'); record(self.repo, output, 'compiler')
        script = "import pathlib,sys; pathlib.Path(sys.argv[-1]).write_bytes(b'partial');sys.exit(1)"
        result = self.invoke(sys.executable, '-c', script, '-o', 'build/probe')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(output.read_bytes(), b'last good')
        self.assertEqual(list((self.repo/'build').glob('.*.staging-*')), [])

    def test_interrupt_preserves_previous_output_and_removes_stage(self):
        output = self.repo/'build/probe'; output.write_bytes(b'last good'); record(self.repo, output, 'compiler')
        script = "import pathlib,sys,time;pathlib.Path(sys.argv[-1]).write_bytes(b'partial');pathlib.Path('ready').touch();time.sleep(20)"
        process = subprocess.Popen(self.command(sys.executable, '-c', script, '-o', 'build/probe'), cwd=self.repo)
        try:
            deadline = time.monotonic()+5
            while not (self.repo/'ready').exists() and time.monotonic()<deadline:
                time.sleep(.01)
            self.assertTrue((self.repo/'ready').exists())
            process.send_signal(signal.SIGTERM)
            self.assertEqual(process.wait(timeout=5), 143)
            self.assertEqual(output.read_bytes(), b'last good')
            self.assertEqual(list((self.repo/'build').glob('.*.staging-*')), [])
        finally:
            if process.poll() is None:
                process.kill(); process.wait()

    def test_unowned_and_changed_predecessors_are_held_before_compiler(self):
        output = self.repo/'build/probe'; output.write_bytes(b'unknown retained bytes')
        compiler = "import pathlib,sys;pathlib.Path('invoked').touch();pathlib.Path(sys.argv[-1]).write_bytes(b'new')"
        for changed in (False, True):
            if changed:
                record(self.repo, output, 'compiler');output.write_bytes(b'changed after receipt')
            before=output.read_bytes()
            result=self.invoke(sys.executable, '-c', compiler, '-o', 'build/probe')
            self.assertEqual(result.returncode,2,result.stderr)
            self.assertFalse((self.repo/'invoked').exists())
            self.assertEqual(output.read_bytes(),before)

    def test_predecessor_changed_by_compiler_is_held_at_publication(self):
        output=self.repo/'build/probe';output.write_bytes(b'owned');record(self.repo,output,'compiler')
        compiler="import pathlib,sys;pathlib.Path('build/probe').write_bytes(b'concurrent change');pathlib.Path(sys.argv[-1]).write_bytes(b'new candidate')"
        result=self.invoke(sys.executable,'-c',compiler,'-o','build/probe')
        self.assertEqual(result.returncode,2,result.stderr)
        self.assertEqual(output.read_bytes(),b'concurrent change')

    def test_unknown_dependency_is_held_without_replacing_owned_object(self):
        output=self.repo/'build/probe.o';output.write_bytes(b'owned');record(self.repo,output,'compiler')
        dependency=output.with_suffix('.d');dependency.write_bytes(b'unknown dependency')
        result=self.invoke('clang','-MMD','-c','missing.c','-o','build/probe.o')
        self.assertEqual(result.returncode,2,result.stderr)
        self.assertEqual(output.read_bytes(),b'owned')
        self.assertEqual(dependency.read_bytes(),b'unknown dependency')

    def test_compiler_shutdown_does_not_poll_or_reap_anchor_before_signaling(self):
        wrapper = """import sys
sys.path.insert(0,'scripts')
import atomic_output as a
class Child(a.subprocess.Popen):
    def poll(self):raise AssertionError('Direct anchor must remain unreaped')
a.subprocess.Popen=Child
raise SystemExit(a.supervise_compiler([sys.executable,'-c','pass']))
"""
        result = subprocess.run([sys.executable,'-B','-c',wrapper],cwd=self.repo,
                                capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_cleanup_refusal_holds_stage_and_preserves_predecessor(self):
        output=self.repo/'build/probe';output.write_bytes(b'last good');record(self.repo,output,'compiler')
        wrapper = """import sys
from pathlib import Path
sys.path.insert(0,'scripts')
import atomic_output as a
def refuse(*args):raise ValueError('Injected cleanup identity refusal')
a.signal_compiler_group=refuse
try:a.run([sys.executable,'-c',"import pathlib,sys;pathlib.Path(sys.argv[-1]).write_bytes(b'candidate')",'-o','build/probe'],Path.cwd())
except ValueError as e:print(e);raise SystemExit(2)
"""
        result=subprocess.run([sys.executable,'-B','-c',wrapper],cwd=self.repo,
                              capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,2,result.stderr)
        self.assertIn('terminal ownership unverified',result.stdout)
        self.assertEqual(output.read_bytes(),b'last good')
        stages=list((self.repo/'build').glob('.probe.staging-*'))
        self.assertEqual(len(stages),1)
        self.assertEqual((stages[0]/'probe').read_bytes(),b'candidate')

    def test_term_ignoring_compiler_has_bounded_shutdown_and_held_stage(self):
        output=self.repo/'build/probe';output.write_bytes(b'last good');record(self.repo,output,'compiler')
        script="import signal,pathlib,sys,time;signal.signal(signal.SIGTERM,signal.SIG_IGN);pathlib.Path(sys.argv[-1]).write_bytes(b'partial');pathlib.Path('ready').touch();time.sleep(30)"
        process=subprocess.Popen(self.command(sys.executable,'-c',script,'-o','build/probe'),cwd=self.repo,
                                 stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            deadline=time.monotonic()+5
            while not (self.repo/'ready').exists() and time.monotonic()<deadline:time.sleep(.01)
            self.assertTrue((self.repo/'ready').exists())
            process.send_signal(signal.SIGTERM)
            out,err=process.communicate(timeout=8)
            self.assertEqual(process.returncode,2,err)
            self.assertIn('terminal ownership unverified',err)
            self.assertEqual(output.read_bytes(),b'last good')
            self.assertEqual(len(list((self.repo/'build').glob('.probe.staging-*'))),1)
        finally:
            if process.poll() is None:process.kill();process.communicate(timeout=5)

    def test_parent_death_stops_group_and_retains_unpublished_stage(self):
        output=self.repo/'build/probe';output.write_bytes(b'last good');record(self.repo,output,'compiler')
        script="import pathlib,sys,time;pathlib.Path(sys.argv[-1]).write_bytes(b'partial');pathlib.Path('ready').touch();time.sleep(1.5);pathlib.Path('unexpected-after-parent-death').touch()"
        process=subprocess.Popen(self.command(sys.executable,'-c',script,'-o','build/probe'),cwd=self.repo,
                                 stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            deadline=time.monotonic()+5
            while not (self.repo/'ready').exists() and time.monotonic()<deadline:time.sleep(.01)
            self.assertTrue((self.repo/'ready').exists())
            process.kill();process.wait(timeout=5)
            time.sleep(1.7)
            self.assertFalse((self.repo/'unexpected-after-parent-death').exists())
            self.assertEqual(output.read_bytes(),b'last good')
            self.assertEqual(len(list((self.repo/'build').glob('.probe.staging-*'))),1)
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=5)

    def test_source_and_symlink_outputs_are_refused_before_command(self):
        source=self.repo/'probe.c';source.write_bytes(b'preserved')
        (self.repo/'build/alias').symlink_to(source)
        for output in ('probe.c', 'build/alias', 'build/alias/file'):
            result=self.invoke('clang', '-o', output, 'missing.c')
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), b'preserved')


if __name__ == '__main__':
    unittest.main()
